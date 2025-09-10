// Renderer.cpp — 修正版（详注）
#include "Renderer.h"
#include "Camera.h"

// === bgfx 头：优先公共入口，内部会包含所需声明 ===
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/math.h>

#include <spdlog/spdlog.h>
#include <vector>

#include "gfx/shader_utils.h" // ke_loadProgramDx11
#include <SDL_syswm.h>        // SDL_SysWMinfo / SDL_GetWindowWMInfo

#include "TextureLoader.h"
#include "GltfLoader.h"  // MeshData / MeshVertex / loadGltfMesh(...)
#include "scene/Scene.h" // Scene / MeshComp（你自己的定义）

// ========== 文件私有静态（光照 uniform） ==========
static bgfx::UniformHandle s_uLightDir = BGFX_INVALID_HANDLE;

// ========== 两套演示用顶点格式（纯色 / 贴图） ==========
struct PosColorVertex
{
    float x, y, z;
    uint32_t abgr;

    static void init(bgfx::VertexLayout &layout)
    {
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true, true)
            .end();
    }
};

struct PosColorTexVertex
{
    float x, y, z;
    uint32_t abgr;
    float u, v;

    static void init(bgfx::VertexLayout &layout)
    {
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true, true)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
    }
};

// ========== 平台窗口句柄 ==========
void *Renderer::nativeWindowHandle(SDL_Window *win)
{
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (SDL_GetWindowWMInfo(win, &wmi) != SDL_TRUE)
        return nullptr;

#if defined(_WIN32)
    return wmi.info.win.window;
#elif defined(__APPLE__)
    return wmi.info.cocoa.window;
#else
    return (void *)(uintptr_t)wmi.info.x11.window;
#endif
}

// ========== 初始化 / 反初始化 ==========
bool Renderer::init(SDL_Window *window, int width, int height)
{
    width_ = width;
    height_ = height;

    // 1) 取原生窗口句柄（nwh），并打印出来用于定位“无窗/黑屏”
    void *nwh = nativeWindowHandle(window);
    spdlog::info("[Renderer] native window handle (nwh) = {}", (void *)nwh);
    if (!nwh)
    {
        spdlog::error("[Renderer] nativeWindowHandle(window) returned null. "
                      "Check SDL_CreateWindow flags (must include SDL_WINDOW_SHOWN).");
        return false;
    }

    // 2) 配置 bgfx::Init
    bgfx::Init init{};
#if BX_PLATFORM_WINDOWS
    // 在 Windows 上，先固定 D3D11 以便排查（确认有画面后可改回 Count 让 bgfx 自选）
    init.type = bgfx::RendererType::Direct3D11;
#else
    init.type = bgfx::RendererType::Count; // 其它平台自动选择
#endif
    init.platformData.nwh = nwh; // ← 关键：窗口句柄
    init.resolution.width = static_cast<uint32_t>(width_);
    init.resolution.height = static_cast<uint32_t>(height_);
    init.resolution.reset = BGFX_RESET_VSYNC; // 打开垂直同步，避免撕裂

    // 3) 初始化 bgfx
    if (!bgfx::init(init))
    {
        spdlog::error("bgfx init failed");
        return false;
    }

    // 4) Debug/HUD 文本开关
    dbgFlags_ = BGFX_DEBUG_TEXT; // 默认开文字（F1/F2/F3 可以切）
    bgfx::setDebug(dbgFlags_);

    // 5) 配置主视图：名称、清屏、视口
    bgfx::setViewName(view_, "main");
    bgfx::setViewClear(view_, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0x303030ff /*灰底*/, 1.0f, 0);
    bgfx::setViewRect(view_, 0, 0,
                      static_cast<uint16_t>(width_),
                      static_cast<uint16_t>(height_));

    // 6) 即使本帧不提交 draw call，也要 touch 一下确保清屏发生
    bgfx::touch(view_);

    // 7) 资源加载（Program/Geometry/Texture）
    if (!createPipelines())
        return false;
    if (!createGeometry())
        return false;
    if (!createTexture())
        return false;

    spdlog::info("[Renderer] init OK ({}x{})", width_, height_);
    return true;
}

void Renderer::shutdown()
{
    destroyTexture();
    destroyGeometry();
    destroyPipelines();
    if (bgfx::isValid(uPointPosRad_))
    {
        bgfx::destroy(uPointPosRad_);
        uPointPosRad_ = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(uPointColInt_))
    {
        bgfx::destroy(uPointColInt_);
        uPointColInt_ = BGFX_INVALID_HANDLE;
    }

    bgfx::shutdown();
    for (auto &kv : texCache_)
    {
        if (bgfx::isValid(kv.second))
            bgfx::destroy(kv.second);
    }
    texCache_.clear();
}

void Renderer::resize(int width, int height)
{
    width_ = width;
    height_ = height;
    bgfx::reset((uint32_t)width_, (uint32_t)height_, BGFX_RESET_VSYNC);
    bgfx::setViewRect(view_, 0, 0, (uint16_t)width_, (uint16_t)height_);
}

// ========== Debug 切换 ==========
void Renderer::setDebug(uint32_t flags)
{
    dbgFlags_ = flags;
    bgfx::setDebug(dbgFlags_);
}
void Renderer::toggleDebug(uint32_t mask)
{
    dbgFlags_ ^= mask;
    bgfx::setDebug(dbgFlags_);
}

// ========== Shader / Program ==========
bool Renderer::createPipelines()
{
    spdlog::info("Loading shaders ...");

    // 基础 program
    progColor_ = ke_loadProgramDx11("vs_simple.bin", "fs_simple.bin");
    progTex_ = ke_loadProgramDx11("vs_tex.bin", "fs_tex.bin");
    if (!bgfx::isValid(progColor_) || !bgfx::isValid(progTex_))
    {
        spdlog::error("Failed to load shader programs.");
        return false;
    }

    // Mesh program（position/normal/uv + 贴图）
    programMesh_ = ke_loadProgramDx11("vs_mesh.bin", "fs_mesh.bin");
    if (!bgfx::isValid(programMesh_))
    {
        spdlog::error("Failed to load mesh program (vs_mesh/fs_mesh)");
        // 不中断，仍可画基础图元
    }

    // Mesh 顶点布局（与你的 MeshVertex 对齐：pos(3f)+normal(3f)+uv(2f)）
    meshLayout_.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    // 光照 uniform（留作扩展）
    // 光照 uniform（用于 Mesh shader 的 Lambert 计算）
    if (!bgfx::isValid(uLightDir_))
        uLightDir_ = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);

    return true;
}

void Renderer::destroyPipelines()
{
    if (bgfx::isValid(progColor_))
    {
        bgfx::destroy(progColor_);
        progColor_ = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(progTex_))
    {
        bgfx::destroy(progTex_);
        progTex_ = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(programMesh_))
    {
        bgfx::destroy(programMesh_);
        programMesh_ = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(s_uLightDir))
    {
        bgfx::destroy(s_uLightDir);
        s_uLightDir = BGFX_INVALID_HANDLE;
    }
}

// ========== 基础几何（演示） ==========
bool Renderer::createGeometry()
{
    bgfx::VertexLayout layoutColor;
    PosColorVertex::init(layoutColor);
    bgfx::VertexLayout layoutTex;
    PosColorTexVertex::init(layoutTex);

    // 颜色三角（无索引）
    const PosColorVertex triVerts[] = {
        {0.0f, 0.6f, 0.0f, 0xff00ffff},
        {0.8f, -0.6f, 0.0f, 0xffffff00},
        {-0.6f, -0.6f, 0.0f, 0xff00ff00},
    };
    vbhTri_ = bgfx::createVertexBuffer(bgfx::copy(triVerts, sizeof(triVerts)), layoutColor);

    // 颜色四边（索引）
    const PosColorVertex quadVerts[] = {
        {-0.8f, 0.6f, 0.0f, 0xff00ffff},
        {0.8f, 0.6f, 0.0f, 0xffffff00},
        {0.8f, -0.6f, 0.0f, 0xff00ff00},
        {-0.8f, -0.6f, 0.0f, 0xff00ffff},
    };
    const uint16_t quadIdx[] = {0, 1, 2, 0, 2, 3};
    vbhQuad_ = bgfx::createVertexBuffer(bgfx::copy(quadVerts, sizeof(quadVerts)), layoutColor);
    ibhQuad_ = bgfx::createIndexBuffer(bgfx::copy(quadIdx, sizeof(quadIdx)));

    // 纹理三角（无索引）
    const PosColorTexVertex triTexVerts[] = {
        {0.0f, 0.6f, 0.0f, 0xffffffff, 0.5f, 0.0f},
        {0.8f, -0.6f, 0.0f, 0xffffffff, 1.0f, 1.0f},
        {-0.6f, -0.6f, 0.0f, 0xffffffff, 0.0f, 1.0f},
    };
    vbhTriTex_ = bgfx::createVertexBuffer(bgfx::copy(triTexVerts, sizeof(triTexVerts)), layoutTex);

    // 纹理四边（复用 quadIdx）
    const PosColorTexVertex quadTexVerts[] = {
        {-0.8f, 0.6f, 0.0f, 0xffffffff, 0.0f, 0.0f},
        {0.8f, 0.6f, 0.0f, 0xffffffff, 1.0f, 0.0f},
        {0.8f, -0.6f, 0.0f, 0xffffffff, 1.0f, 1.0f},
        {-0.8f, -0.6f, 0.0f, 0xffffffff, 0.0f, 1.0f},
    };
    vbhQuadTex_ = bgfx::createVertexBuffer(bgfx::copy(quadTexVerts, sizeof(quadTexVerts)), layoutTex);

    if (!bgfx::isValid(vbhTri_) || !bgfx::isValid(vbhQuad_) || !bgfx::isValid(ibhQuad_) ||
        !bgfx::isValid(vbhTriTex_) || !bgfx::isValid(vbhQuadTex_))
    {
        spdlog::error("Failed to create VB/IB.");
        return false;
    }
    return true;
}

void Renderer::destroyGeometry()
{
    if (bgfx::isValid(vbhTri_))
    {
        bgfx::destroy(vbhTri_);
        vbhTri_ = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(vbhQuad_))
    {
        bgfx::destroy(vbhQuad_);
        vbhQuad_ = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(ibhQuad_))
    {
        bgfx::destroy(ibhQuad_);
        ibhQuad_ = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(vbhTriTex_))
    {
        bgfx::destroy(vbhTriTex_);
        vbhTriTex_ = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(vbhQuadTex_))
    {
        bgfx::destroy(vbhQuadTex_);
        vbhQuadTex_ = BGFX_INVALID_HANDLE;
    }
}

// ========== 纹理 ==========
bool Renderer::createTexture()
{
    uSampler_ = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    samplerFlags_ = uint32_t(BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP |
                             BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT);

    // 先从 docs/image.png 尝试
    std::string path = std::string(KE_ASSET_DIR) + "/image.png";
    tex_ = createTexture2DFromFile(path, /*srgb=*/false, samplerFlags_);
    if (bgfx::isValid(tex_))
    {
        spdlog::info("[Texture] loaded {}", path);
        return true;
    }

    // 回退棋盘
    tex_ = createCheckerTexRGBA8(256, 256, 32);
    if (!bgfx::isValid(uSampler_) || !bgfx::isValid(tex_))
    {
        spdlog::error("Failed to create texture/uniform.");
        return false;
    }
    spdlog::warn("[Texture] fallback to checkerboard");
    return true;
}

void Renderer::destroyTexture()
{
    if (bgfx::isValid(tex_))
    {
        bgfx::destroy(tex_);
        tex_ = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(uSampler_))
    {
        bgfx::destroy(uSampler_);
        uSampler_ = BGFX_INVALID_HANDLE;
    }
}

bgfx::TextureHandle Renderer::createCheckerTexRGBA8(uint16_t w, uint16_t h, uint16_t cell)
{
    std::vector<uint8_t> pixels(size_t(w) * h * 4);
    for (uint16_t y = 0; y < h; ++y)
        for (uint16_t x = 0; x < w; ++x)
        {
            bool odd = ((x / cell) + (y / cell)) & 1;
            uint8_t v = odd ? 220 : 32;
            size_t i = (size_t(y) * w + x) * 4;
            pixels[i + 0] = v;
            pixels[i + 1] = v;
            pixels[i + 2] = v;
            pixels[i + 3] = 255;
        }
    const bgfx::Memory *mem = bgfx::copy(pixels.data(), (uint32_t)pixels.size());
    return bgfx::createTexture2D(w, h, false, 1, bgfx::TextureFormat::RGBA8, 0, mem);
}
bgfx::TextureHandle Renderer::getOrLoadTexture(const std::string &path, bool srgb, uint32_t samplerFlags)
{
    auto it = texCache_.find(path);
    if (it != texCache_.end() && bgfx::isValid(it->second))
    {
        return it->second; // 命中缓存
    }
    bgfx::TextureHandle h = createTexture2DFromFile(path, srgb, samplerFlags);
    if (bgfx::isValid(h))
    {
        texCache_[path] = h;
    }
    return h;
}

// ========== 热重载 ==========
void Renderer::hotReloadShaders()
{
    destroyPipelines();
    createPipelines();
}

// ========== 每帧渲染（演示图元 + Mesh） ==========
void Renderer::renderFrame(Camera &cam, uint32_t &outDraws, uint32_t &outTris, float angle)
{
    // HUD
    bgfx::dbgTextClear();
    bgfx::dbgTextPrintf(0, 0, 0x0f, "Draw(last)=%u  Tris(last)=%u", outDraws, outTris);
    if (showHelp_)
    {
        bgfx::dbgTextPrintf(0, 1, 0x0a, "R: reload  F1/F2/F3: debug panels");
        bgfx::dbgTextPrintf(0, 2, 0x0a, "1: tri  2: quad  3: mesh   O: ortho  P: persp  H: help");
        bgfx::dbgTextPrintf(0, 3, 0x0a, "WASD move | Space up | C down | Shift sprint | RMB drag to look");
        bgfx::dbgTextPrintf(0, 4, 0x0a, "M: load glTF (docs/models/model.gltf) and draw Mesh");
        bgfx::dbgTextPrintf(0, 5, 0x0a, "T: tex on/off   L: reload docs/image.png");
    }

    // 把相机矩阵送给本 View
    cam.apply(view_);
    bgfx::touch(view_);

    // 模型矩阵（绕 Z 旋转）
    float mtxModel[16];
    bx::mtxRotateZ(mtxModel, angle);
    bgfx::setTransform(mtxModel);

    // 优先：Mesh 模式
    if (drawMode_ == DrawMode::Mesh)
    {
        if (meshLoaded_ && bgfx::isValid(programMesh_))
        {
            bgfx::setTexture(0, uSampler_, tex_, samplerFlags_); // fs_mesh 采样用
            bgfx::setVertexBuffer(0, meshVbh_);
            bgfx::setIndexBuffer(meshIbh_);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA | BGFX_STATE_DEPTH_TEST_LESS);
            bgfx::submit(view_, programMesh_);
        }
    }
    else if (!useTex_)
    {
        // 纯色
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA);
        if (drawMode_ == DrawMode::Triangle)
            bgfx::setVertexBuffer(0, vbhTri_, 0, 3);
        else
        {
            bgfx::setVertexBuffer(0, vbhQuad_);
            bgfx::setIndexBuffer(ibhQuad_);
        }
        bgfx::submit(view_, progColor_);
    }
    else
    {
        // 贴图
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA);
        bgfx::setTexture(0, uSampler_, tex_, samplerFlags_);
        if (drawMode_ == DrawMode::Triangle)
            bgfx::setVertexBuffer(0, vbhTriTex_, 0, 3);
        else
        {
            bgfx::setVertexBuffer(0, vbhQuadTex_);
            bgfx::setIndexBuffer(ibhQuad_);
        }
        bgfx::submit(view_, progTex_);
    }

    bgfx::frame();

    // 统计
    if (const bgfx::Stats *st = bgfx::getStats())
    {
#if defined(BGFX_TOPOLOGY_TRI_LIST)
        outTris = st->numPrims[BGFX_TOPOLOGY_TRI_LIST];
#else
        outTris = st->numPrims[(uint8_t)bgfx::Topology::TriList];
#endif
        outDraws = st->numDraw;
    }
}

// ========== 动态更换贴图 ==========
bool Renderer::loadTextureFromFile(const std::string &path)
{
    auto newTex = createTexture2DFromFile(path, /*srgb=*/false, samplerFlags_);
    if (!bgfx::isValid(newTex))
    {
        spdlog::error("[Texture] reload failed: {}", path);
        return false;
    }
    if (bgfx::isValid(tex_))
        bgfx::destroy(tex_);
    tex_ = newTex;
    spdlog::info("[Texture] reloaded: {}", path);
    return true;
}

// ========== 从 glTF 载入 Mesh（创建本类成员 VB/IB） ==========
bool Renderer::loadMeshFromGltf(const std::string &path)
{
    MeshData md{};
    std::string warn, err;
    if (!loadGltfMesh(path, md, &warn, &err))
    {
        spdlog::error("[Renderer] loadGltfMesh failed: {}  warn={}  err={}", path, warn, err);
        return false;
    }

    // 清旧的
    if (bgfx::isValid(meshVbh_))
        bgfx::destroy(meshVbh_);
    if (bgfx::isValid(meshIbh_))
        bgfx::destroy(meshIbh_);
    meshVbh_ = BGFX_INVALID_HANDLE;
    meshIbh_ = BGFX_INVALID_HANDLE;

    // 创建 VBO / IBO（使用你的 MeshVertex / uint16 索引）
    const bgfx::Memory *vbmem =
        bgfx::copy(md.vertices.data(), (uint32_t)(md.vertices.size() * sizeof(MeshVertex)));
    meshVbh_ = bgfx::createVertexBuffer(vbmem, meshLayout_);

    const bgfx::Memory *ibmem =
        bgfx::copy(md.indices.data(), (uint32_t)(md.indices.size() * sizeof(uint16_t)));
    meshIbh_ = bgfx::createIndexBuffer(ibmem);

    meshIndexCount_ = (uint32_t)md.indices.size();

    if (!bgfx::isValid(meshVbh_) || !bgfx::isValid(meshIbh_))
    {
        spdlog::error("[Renderer] create VBO/IBO failed for mesh");
        return false;
    }

    // 若 glTF 有 BaseColor 贴图，替换为该贴图；否则保留当前纹理（棋盘/上次）
    if (!md.baseColorTexPath.empty())
        (void)loadTextureFromFile(md.baseColorTexPath);

    meshLoaded_ = true;
    spdlog::info("[Renderer] mesh loaded. verts={} indices={}", md.vertices.size(), md.indices.size());
    return true;
}

// —— 渲染整个 Scene（Mesh 模式用这个） ——————————————————————————————
void Renderer::renderScene(const Scene &scn, Camera &cam)
{
    // 1) 每帧更新 Scene（把 SRT 写入 model 矩阵）
    const_cast<Scene &>(scn).update();

    // 2) HUD（保留你 Day3 的显示即可）
    bgfx::dbgTextClear();
    if (showHelp_)
    {
        bgfx::dbgTextPrintf(0, 1, 0x0a, "Scene mode: %zu mesh(es)", scn.meshes.size());
        bgfx::dbgTextPrintf(0, 2, 0x0a, "WASD move | Space up | C down | Shift sprint | RMB drag to look");
    }

    // 3) 视口 & 清屏（保持 Day3）
    uint16_t vw = 1280, vh = 720;
    if (const bgfx::Stats *st = bgfx::getStats())
    {
        vw = (uint16_t)st->width;
        vh = (uint16_t)st->height;
    }
    bgfx::setViewName(view_, "main");
    bgfx::setViewClear(view_, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
    bgfx::setViewRect(view_, 0, 0, vw, vh);

    // 4) 应用相机（设置 view/proj 矩阵等）
    cam.apply(view_);
    bgfx::touch(view_);

    // 5) 光照 uniform（与你 fs_mesh.sc 对齐）
    //    u_lightDir: xyz=方向(指向物体)，w=环境光强度[0,1]
    if (bgfx::isValid(uLightDir_))
    {
        bgfx::setUniform(uLightDir_, lightDir_);
    }

    //    点光（可选）：u_pointPosRad / u_pointColInt
    //    若未启用或句柄无效，则传 0 向量，保证不影响现有渲染
    float zero4[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    if (bgfx::isValid(uPointPosRad_) && bgfx::isValid(uPointColInt_) && pointEnabled_)
    {
        bgfx::setUniform(uPointPosRad_, &pointPosRad_[0]); // pos.xyz, radius
        bgfx::setUniform(uPointColInt_, &pointColInt_[0]); // color.xyz, intensity
    }
    else
    {
        if (bgfx::isValid(uPointPosRad_))
            bgfx::setUniform(uPointPosRad_, zero4);
        if (bgfx::isValid(uPointColInt_))
            bgfx::setUniform(uPointColInt_, zero4);
    }

    // 6) 提交场景中的所有网格
    for (auto const &mc : scn.meshes)
    {
        if (!bgfx::isValid(mc.vbh) || !bgfx::isValid(mc.ibh))
            continue;

        // 渲染状态（保留你每个 mesh 的自定义 state）
        bgfx::setState(mc.state);
        bgfx::setTransform(mc.model); // 来自 Scene.update()
        bgfx::setVertexBuffer(0, mc.vbh);
        bgfx::setIndexBuffer(mc.ibh);

        // 贴图（槽位0，采样器与 fs_mesh.sc 的 s_texColor 对齐）
        if (bgfx::isValid(mc.baseColor) && bgfx::isValid(uSampler_))
            bgfx::setTexture(0, uSampler_, mc.baseColor, samplerFlags_);

        // 提交
        if (bgfx::isValid(programMesh_))
            bgfx::submit(view_, programMesh_);
    }

    // 7) 结束一帧（保持 Day3）
    bgfx::frame();
}

// —— 把 glTF 载入为 MeshComp 推到 Scene ——————————————————————————

bool Renderer::addMeshFromGltfToScene(const std::string &path, Scene &scn)
{
    MeshData md{};
    std::string warn, err;
    if (!loadGltfMesh(path, md, &warn, &err))
    {
        spdlog::error("[Renderer] addMeshFromGltfToScene: loadGltfMesh failed: {}  warn={} err={}",
                      path, warn, err);
        return false;
    }

    // 顶点/索引缓冲（MeshVertex：pos3f normal3f uv2f）
    const bgfx::Memory *vmem =
        bgfx::copy(md.vertices.data(), (uint32_t)(md.vertices.size() * sizeof(MeshVertex)));
    bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(vmem, meshLayout_);

    const bgfx::Memory *imem =
        bgfx::copy(md.indices.data(), (uint32_t)(md.indices.size() * sizeof(uint16_t)));
    bgfx::IndexBufferHandle ibh = bgfx::createIndexBuffer(imem);

    if (!bgfx::isValid(vbh) || !bgfx::isValid(ibh))
    {
        spdlog::error("[Renderer] addMeshFromGltfToScene: VB/IB invalid");
        return false;
    }

    MeshComp mc{};
    mc.vbh = vbh;
    mc.ibh = ibh;
    mc.indexCount = static_cast<uint32_t>(md.indices.size());
    // 默认渲染状态（深度写入 + 深度测试 + MSAA）
    mc.state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_MSAA | BGFX_STATE_DEPTH_TEST_LESS;
    // 初始化变换矩阵为单位阵
    bx::mtxIdentity(mc.model);

    // —— GPU 贴图（实时渲染用）——
    if (!md.baseColorTexPath.empty())
    {
        bgfx::TextureHandle htex =
            createTexture2DFromFile(md.baseColorTexPath, /*srgb=*/false, samplerFlags_);
        if (bgfx::isValid(htex))
            mc.baseColor = htex;
    }

    // —— CPU 缓存（给 Exporter 用）——
    mc.cpuVertices.resize(md.vertices.size());
    for (size_t i = 0; i < md.vertices.size(); ++i)
    {
        const auto &s = md.vertices[i];
        auto &d = mc.cpuVertices[i];
        d.px = s.px;
        d.py = s.py;
        d.pz = s.pz;
        d.nx = s.nx;
        d.ny = s.ny;
        d.nz = s.nz;
        d.u = s.u;
        d.v = s.v;
    }
    mc.cpuIndices.resize(md.indices.size());
    for (size_t i = 0; i < md.indices.size(); ++i)
    {
        mc.cpuIndices[i] = static_cast<uint32_t>(md.indices[i]);
    }
    mc.baseColorPath = md.baseColorTexPath; // 字符串保存贴图路径（Exporter 写 MTL 用）

    // —— 加入场景 ——
    scn.meshes.push_back(std::move(mc));

    spdlog::info("[Renderer] addMeshFromGltfToScene: verts={} indices={} path={}",
                 md.vertices.size(), md.indices.size(), path);
    return true;
}
void Renderer::setPointLightEnabled(bool enabled)
{
    pointEnabled_ = enabled;
    if (!enabled)
    {
        pointPosRad_[0] = pointPosRad_[1] = pointPosRad_[2] = pointPosRad_[3] = 0.0f;
        pointColInt_[0] = pointColInt_[1] = pointColInt_[2] = pointColInt_[3] = 0.0f;
    }
}

void Renderer::setPointLight(float px, float py, float pz, float radius,
                             float cr, float cg, float cb, float intensity)
{
    pointPosRad_[0] = px;
    pointPosRad_[1] = py;
    pointPosRad_[2] = pz;
    pointPosRad_[3] = (radius > 0.f ? radius : 0.f);
    pointColInt_[0] = cr;
    pointColInt_[1] = cg;
    pointColInt_[2] = cb;
    pointColInt_[3] = (intensity > 0.f ? intensity : 0.f);
}
