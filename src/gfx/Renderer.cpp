#include "Renderer.h"
#include "Camera.h"
#include <bgfx/platform.h>
#include <bx/math.h>
#include <spdlog/spdlog.h>
#include <vector>
#include "gfx/shader_utils.h" // ke_loadProgramDx11
//  新增：没有它，SDL_SysWMinfo / SDL_GetWindowWMInfo 都是未声明
#include <SDL_syswm.h>
#include "TextureLoader.h"
#include "GltfLoader.h"
// 顶点格式（颜色）
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
// 顶点格式（带纹理）
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

bool Renderer::init(SDL_Window *window, int width, int height)
{
    width_ = width;
    height_ = height;

    bgfx::Init init{};
    init.type = bgfx::RendererType::Count;
    init.platformData.nwh = nativeWindowHandle(window);
    init.resolution.width = (uint32_t)width_;
    init.resolution.height = (uint32_t)height_;
    init.resolution.reset = BGFX_RESET_VSYNC;

    if (!bgfx::init(init))
    {
        spdlog::error("bgfx init failed");
        return false;
    }

    dbgFlags_ = BGFX_DEBUG_TEXT; // 默认只显示文字
    bgfx::setDebug(dbgFlags_);

    bgfx::setViewName(view_, "main");
    bgfx::setViewClear(view_, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
    bgfx::setViewRect(view_, 0, 0, (uint16_t)width_, (uint16_t)height_);
    bgfx::touch(view_);

    if (!createPipelines())
        return false;
    if (!createGeometry())
        return false;
    if (!createTexture())
        return false;

    return true;
}

void Renderer::shutdown()
{
    destroyTexture();
    destroyGeometry();
    destroyPipelines();

    bgfx::shutdown();
}

void Renderer::resize(int width, int height)
{
    width_ = width;
    height_ = height;
    bgfx::reset((uint32_t)width_, (uint32_t)height_, BGFX_RESET_VSYNC);
    bgfx::setViewRect(view_, 0, 0, (uint16_t)width_, (uint16_t)height_);
}

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

bool Renderer::createPipelines()
{
    spdlog::info("Loading shaders ...");
    progColor_ = ke_loadProgramDx11("vs_simple.bin", "fs_simple.bin");
    progTex_ = ke_loadProgramDx11("vs_tex.bin", "fs_tex.bin");
    if (!bgfx::isValid(progColor_) || !bgfx::isValid(progTex_))
    {
        spdlog::error("Failed to load shader programs.");
        return false;
    }
    // === Mesh 管线（position/normal/uv + 贴图）===
    programMesh_ = ke_loadProgramDx11("vs_mesh.bin", "fs_mesh.bin");
    if (!bgfx::isValid(programMesh_))
    {
        spdlog::error("Failed to load mesh program (vs_mesh/fs_mesh)");
        // 非致命：你也可以 return false; 这里我选择继续跑（只是不能画 Mesh）
    }

    // 初始化 Mesh 顶点布局
    meshLayout_.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

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
}

bool Renderer::createGeometry()
{
    // 布局
    bgfx::VertexLayout layoutColor;
    PosColorVertex::init(layoutColor);
    bgfx::VertexLayout layoutTex;
    PosColorTexVertex::init(layoutTex);

    // 颜色三角（无索引）
    const PosColorVertex triVerts[] = {
        {0.0f, 0.6f, 0.0f, 0xff00ffff},
        {0.8f, -0.6f, 0.0f, 0xffffff00},
        {-0.6f, -0.6f, 0.0f, 0xff00ff00}};
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

    if (!bgfx::isValid(vbhTri_) || !bgfx::isValid(vbhQuad_) || !bgfx::isValid(ibhQuad_) || !bgfx::isValid(vbhTriTex_) || !bgfx::isValid(vbhQuadTex_))
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

bool Renderer::createTexture()
{
    uSampler_ = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);

    samplerFlags_ = static_cast<uint32_t>(
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT);

    // 先尝试从 docs/image.png 加载
    std::string path = std::string(KE_ASSET_DIR) + "/image.png";
    tex_ = createTexture2DFromFile(path, /*srgb=*/false, samplerFlags_);
    if (bgfx::isValid(tex_))
    {
        spdlog::info("[Texture] loaded {}", path);
        return true;
    }

    // 失败则回退到棋盘
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
    {
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
    }
    const bgfx::Memory *mem = bgfx::copy(pixels.data(), (uint32_t)pixels.size());
    return bgfx::createTexture2D(w, h, false, 1, bgfx::TextureFormat::RGBA8, 0, mem);
}

void Renderer::hotReloadShaders()
{
    destroyPipelines();
    createPipelines();
}

void Renderer::renderFrame(Camera &cam, uint32_t &outDraws, uint32_t &outTris, float angle)
{
    // HUD（显示上一帧数据）
    bgfx::dbgTextClear();
    bgfx::dbgTextPrintf(0, 3, 0x0a, "M: load glTF (docs/models/model.gltf) and draw Mesh");
    bgfx::dbgTextPrintf(0, 0, 0x0f, "Draw(last)=%u  Tris(last)=%u", outDraws, outTris);
    if (showHelp_)
    {
        bgfx::dbgTextPrintf(0, 1, 0x0a, "R: reload  F1/F2/F3: debug  T: tex on/off");
        bgfx::dbgTextPrintf(0, 2, 0x0a, "1: triangle  2: quad   O: ortho  P: persp  H: help");
    }

    cam.apply(view_);
    bgfx::touch(view_);

    float model[16];
    bx::mtxRotateZ(model, angle);
    bgfx::setTransform(model);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA);

    // 模型矩阵（绕 Z 旋转）
    float mtxModel[16];
    bx::mtxRotateZ(mtxModel, angle);
    bgfx::setTransform(mtxModel);

    // 优先：Mesh 模式（独立于 useTex_ 开关）
    if (drawMode_ == DrawMode::Mesh)
    {
        if (meshLoaded_ && bgfx::isValid(programMesh_))
        {
            // Mesh 走贴图管线（fs_mesh 取 s_texColor）
            bgfx::setTexture(0, uSampler_, tex_, samplerFlags_);
            bgfx::setVertexBuffer(0, meshVbh_);
            bgfx::setIndexBuffer(meshIbh_);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA | BGFX_STATE_DEPTH_TEST_LESS);
            bgfx::submit(view_, programMesh_);
        }
    }
    else if (!useTex_)
    { // 你原来的“纯色”分支
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA);
        if (drawMode_ == DrawMode::Triangle)
        {
            bgfx::setVertexBuffer(0, vbhTri_, 0, 3);
        }
        else
        {
            bgfx::setVertexBuffer(0, vbhQuad_);
            bgfx::setIndexBuffer(ibhQuad_);
        }
        bgfx::submit(view_, progColor_);
    }
    else
    { // 你原来的“贴图”分支
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA);
        bgfx::setTexture(0, uSampler_, tex_, samplerFlags_);
        if (drawMode_ == DrawMode::Triangle)
        {
            bgfx::setVertexBuffer(0, vbhTriTex_, 0, 3);
        }
        else
        {
            bgfx::setVertexBuffer(0, vbhQuadTex_);
            bgfx::setIndexBuffer(ibhQuad_);
        }
        bgfx::submit(view_, progTex_);
    }

    bgfx::frame();

    // 统计当前帧，交回给上层用于“下一帧”打印
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
bool Renderer::loadMeshFromGltf(const std::string &path)
{
    MeshData md;
    std::string w, e;
    if (!loadGltfMesh(path, md, &w, &e))
    {
        spdlog::error("[Renderer] loadGltfMesh failed: {}", path);
        return false;
    }

    // 先清旧的
    if (bgfx::isValid(meshVbh_))
        bgfx::destroy(meshVbh_);
    if (bgfx::isValid(meshIbh_))
        bgfx::destroy(meshIbh_);
    meshVbh_ = BGFX_INVALID_HANDLE;
    meshIbh_ = BGFX_INVALID_HANDLE;

    // 创建 VBO/IBO
    const bgfx::Memory *vbmem = bgfx::copy(md.vertices.data(), (uint32_t)(md.vertices.size() * sizeof(MeshVertex)));
    meshVbh_ = bgfx::createVertexBuffer(vbmem, meshLayout_);
    const bgfx::Memory *ibmem = bgfx::copy(md.indices.data(), (uint32_t)(md.indices.size() * sizeof(uint16_t)));
    meshIbh_ = bgfx::createIndexBuffer(ibmem);
    meshIndexCount_ = (uint32_t)md.indices.size();

    if (!bgfx::isValid(meshVbh_) || !bgfx::isValid(meshIbh_))
    {
        spdlog::error("[Renderer] create VBO/IBO failed for mesh");
        return false;
    }

    // 贴图：若 glTF 有外链 BaseColor，读它；否则保留当前纹理（棋盘/上次的）
    if (!md.baseColorTexPath.empty())
    {
        loadTextureFromFile(md.baseColorTexPath);
    }

    meshLoaded_ = true;
    spdlog::info("[Renderer] mesh loaded. verts={} indices={}", md.vertices.size(), md.indices.size());
    return true;
}
