// Renderer.cpp — 接回glTF加载与Scene渲染（保留演示路径）
#include "Renderer.h"
#include <SDL.h>
#include <SDL_syswm.h>

#include <bgfx/platform.h>
#include <bx/math.h>
#include <spdlog/spdlog.h>
#include <vector>

#include "gfx/shaders/shader_utils.h"
#include "gfx/texture/TextureLoader.h"
#include "io/gltf/GltfLoader.h" // 用你的加载器

// 仅用于本文件的小结构：记录已上传到GPU的网格
struct LoadedMesh
{
    bgfx::VertexBufferHandle vbh{BGFX_INVALID_HANDLE};
    bgfx::IndexBufferHandle ibh{BGFX_INVALID_HANDLE};
    bgfx::TextureHandle tex{BGFX_INVALID_HANDLE};
    float model[16]{};
};

// 渲染器内部缓存（兼容层，不侵入你的 Scene）
static std::vector<LoadedMesh> s_loadedMeshes;

// ========== 平台：从 SDL 窗口取原生句柄 ==========
static void *sdlNativeWindow(SDL_Window *win)
{
    SDL_SysWMinfo wmi{};
    SDL_VERSION(&wmi.version);
    if (!SDL_GetWindowWMInfo(win, &wmi))
    {
        spdlog::error("SDL_GetWindowWMInfo failed: {}", SDL_GetError());
        return nullptr;
    }
#if defined(_WIN32)
    return wmi.info.win.window;
#elif defined(__APPLE__) && defined(SDL_VIDEO_DRIVER_COCOA)
    return wmi.info.cocoa.window;
#else
    return (void *)(uintptr_t)wmi.info.x11.window;
#endif
}

// ========== 生命周期 ==========
bool Renderer::init(SDL_Window *window, int width, int height)
{
    if (!window)
    {
        spdlog::error("Renderer::init: window is null");
        return false;
    }

    SDL_ShowWindow(window);

    int w = width, h = height;
#if SDL_VERSION_ATLEAST(2, 0, 5)
    if (w <= 0 || h <= 0)
        SDL_GL_GetDrawableSize(window, &w, &h);
#endif
    if (w <= 0 || h <= 0)
        SDL_GetWindowSize(window, &w, &h);
    if (w <= 0 || h <= 0)
    {
        w = 1280;
        h = 720;
    }

    width_ = (uint32_t)w;
    height_ = (uint32_t)h;

    void *nwh = sdlNativeWindow(window);
    spdlog::info("Renderer::init: HWND={}", (void *)nwh);
    if (!nwh)
    {
        spdlog::error("Renderer::init: native handle is null");
        return false;
    }

    bgfx::PlatformData pd{};
    pd.ndt = nullptr;
    pd.nwh = nwh;
    pd.context = nullptr;
    pd.backBuffer = nullptr;
    pd.backBufferDS = nullptr;

    bgfx::Init init{};
#if BX_PLATFORM_WINDOWS
    init.type = bgfx::RendererType::Direct3D11;
#else
    init.type = bgfx::RendererType::Count; // 其他平台自动选择
#endif
    init.resolution.width = width_;
    init.resolution.height = height_;
    init.resolution.reset = BGFX_RESET_VSYNC;
    init.platformData = pd;

    if (!bgfx::init(init))
    {
        spdlog::error("bgfx::init failed ({}x{}), HWND={}", width_, height_, (void *)nwh);
        return false;
    }

    bgfx::setViewRect(viewId_, 0, 0, width_, height_);
    bgfx::setViewClear(viewId_, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

    if (!createPipelines())
        spdlog::error("createPipelines failed");
    if (!createGeometry())
        spdlog::warn("createGeometry failed");
    if (!createTexture())
        spdlog::warn("createTexture failed");

    pbr_.init();
    matMgr_.init();

    spdlog::info("Renderer init OK ({}x{}), hwnd={}", width_, height_, (void *)nwh);
    return true;
}

void Renderer::shutdown()
{
    // 清掉兼容层里缓存的网格
    for (auto &m : s_loadedMeshes)
    {
        if (bgfx::isValid(m.vbh))
            bgfx::destroy(m.vbh);
        if (bgfx::isValid(m.ibh))
            bgfx::destroy(m.ibh);
        // 纹理由 ResourceCache 复用/释放，这里只销毁临时生成的棋盘纹理
    }
    s_loadedMeshes.clear();

    destroyTexture();
    destroyGeometry();
    destroyPipelines();

    pbr_.shutdown();
    matMgr_.shutdown();
    resCache_.clear();

    bgfx::shutdown();
}

void Renderer::resize(int width, int height)
{
    width_ = (uint32_t)width;
    height_ = (uint32_t)height;
    bgfx::reset(width_, height_, BGFX_RESET_VSYNC);
    bgfx::setViewRect(viewId_, 0, 0, width_, height_);
}

void Renderer::setDebug(uint32_t flags)
{
    bgfx::setDebug(flags);
}

// ========== 灯光（转发给 ForwardPBR::lighting） ==========
void Renderer::setLightDir(float x, float y, float z, float ambient)
{
    pbr_.lighting().lightDir_ambient = {x, y, z, ambient};
}
void Renderer::setPointLightEnabled(bool enabled)
{
    if (!enabled)
        pbr_.lighting().pointPos_radius.w = 0.0f;
}
void Renderer::setPointLight(float px, float py, float pz, float radius,
                             float cr, float cg, float cb, float intensity)
{
    pbr_.lighting().pointPos_radius = {px, py, pz, radius};
    pbr_.lighting().pointCol_intensity = {cr, cg, cb, intensity};
}
void Renderer::setPointLight(const float pos[3], float radius,
                             const float color[3], float intensity)
{
    setPointLight(pos[0], pos[1], pos[2], radius, color[0], color[1], color[2], intensity);
}
void Renderer::setViewPos(float x, float y, float z)
{
    pbr_.lighting().viewPos_exposure.x = x;
    pbr_.lighting().viewPos_exposure.y = y;
    pbr_.lighting().viewPos_exposure.z = z;
}
void Renderer::setExposure(float e)
{
    pbr_.lighting().viewPos_exposure.w = e;
}

// ========== PBR（先留接口，稍后正式接入） ==========
PbrMatHandle Renderer::createPbrMaterial(const PbrMaterialDesc &d)
{
    auto h = matMgr_.create(d);
    auto &gpu = const_cast<PbrMaterialGPU &>(matMgr_.get(h));
    pbr_.attachProgramTo(gpu);
    return h;
}

#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
void Renderer::drawMeshPBR(const float *modelMtx,
                           bgfx::VertexBufferHandle vbh,
                           bgfx::IndexBufferHandle ibh,
                           PbrMatHandle h,
                           uint8_t viewId)
{
    glm::mat4 M = glm::make_mat4(modelMtx);
    pbr_.draw(M, vbh, ibh, matMgr_.get(h), viewId);
}

// ========== 内置 shader/几何 ==========
bool Renderer::createPipelines()
{
    programSimple_ = ke_loadProgramDx11("vs_simple.bin", "fs_simple.bin");
    programTex_ = ke_loadProgramDx11("vs_tex.bin", "fs_tex.bin");
    programMesh_ = ke_loadProgramDx11("vs_mesh.bin", "fs_mesh.bin");
    return true;
}
void Renderer::destroyPipelines()
{
    if (bgfx::isValid(programSimple_))
    {
        bgfx::destroy(programSimple_);
        programSimple_ = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(programTex_))
    {
        bgfx::destroy(programTex_);
        programTex_ = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(programMesh_))
    {
        bgfx::destroy(programMesh_);
        programMesh_ = BGFX_INVALID_HANDLE;
    }
}

bool Renderer::createGeometry()
{
    // 演示三角与四边形：用局部布局（Position+Color0）
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();

    struct Vtx
    {
        float x, y, z;
        uint32_t abgr;
    };

    // 三角形
    {
        Vtx tri[3] = {
            {0.0f, 0.5f, 0.0f, 0xff00ffff},
            {-0.5f, -0.5f, 0.0f, 0xffff00ff},
            {0.5f, -0.5f, 0.0f, 0xffffffff},
        };
        const bgfx::Memory *vm = bgfx::copy(tri, sizeof(tri));
        vbhTri_ = bgfx::createVertexBuffer(vm, layout);
        uint16_t idx[3] = {0, 1, 2};
        const bgfx::Memory *im = bgfx::copy(idx, sizeof(idx));
        ibhTri_ = bgfx::createIndexBuffer(im);
    }
    // 四边形
    {
        Vtx quad[4] = {
            {-0.5f, 0.5f, 0.0f, 0xff0000ff},
            {0.5f, 0.5f, 0.0f, 0xff00ff00},
            {0.5f, -0.5f, 0.0f, 0xffff0000},
            {-0.5f, -0.5f, 0.0f, 0xffffffff},
        };
        uint16_t idx[6] = {0, 1, 2, 0, 2, 3};
        const bgfx::Memory *vm = bgfx::copy(quad, sizeof(quad));
        vbhQuad_ = bgfx::createVertexBuffer(vm, layout);
        const bgfx::Memory *im = bgfx::copy(idx, sizeof(idx));
        ibhQuad_ = bgfx::createIndexBuffer(im);
    }
    return bgfx::isValid(vbhTri_) && bgfx::isValid(ibhTri_) &&
           bgfx::isValid(vbhQuad_) && bgfx::isValid(ibhQuad_);
}
void Renderer::destroyGeometry()
{
    if (bgfx::isValid(vbhTri_))
    {
        bgfx::destroy(vbhTri_);
        vbhTri_ = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(ibhTri_))
    {
        bgfx::destroy(ibhTri_);
        ibhTri_ = BGFX_INVALID_HANDLE;
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
}

bgfx::TextureHandle Renderer::createCheckerTexRGBA8(uint16_t w, uint16_t h, uint16_t cell)
{
    std::vector<uint8_t> pixels(w * h * 4);
    for (uint16_t y = 0; y < h; ++y)
        for (uint16_t x = 0; x < w; ++x)
        {
            bool on = ((x / cell) + (y / cell)) % 2 == 0;
            uint8_t r = on ? 255 : 30, g = on ? 255 : 30, b = on ? 255 : 30;
            size_t i = (size_t(y) * w + x) * 4;
            pixels[i + 0] = r;
            pixels[i + 1] = g;
            pixels[i + 2] = b;
            pixels[i + 3] = 255;
        }
    const bgfx::Memory *mem = bgfx::copy(pixels.data(), (uint32_t)pixels.size());
    return bgfx::createTexture2D(w, h, false, 1, bgfx::TextureFormat::RGBA8, 0, mem);
}

bool Renderer::createTexture()
{
    texDemo_ = createCheckerTexRGBA8(256, 256, 32);
    return bgfx::isValid(texDemo_);
}
void Renderer::destroyTexture()
{
    if (bgfx::isValid(texDemo_))
    {
        bgfx::destroy(texDemo_);
        texDemo_ = BGFX_INVALID_HANDLE;
    }
}

bool Renderer::loadTextureFromFile(const std::string &path)
{
    auto t = createTexture2DFromFile(path, /*srgb=*/false, 0);
    if (bgfx::isValid(t))
    {
        if (bgfx::isValid(texDemo_))
            bgfx::destroy(texDemo_);
        texDemo_ = t;
        return true;
    }
    return false;
}

// ========== glTF → Renderer 内部缓存 ==========
bool Renderer::loadMeshFromGltf(const std::string &path)
{
    // 保留接口；真正把网格塞进渲染的逻辑在 addMeshFromGltfToScene
    return addMeshFromGltfToScene(path, *(Scene *)nullptr);
}

bool Renderer::addMeshFromGltfToScene(const std::string &path, Scene &)
{
    MeshData md;
    std::string baseColorPath;
    std::string normalPath;
    if (!loadGltfMesh(path, md, &baseColorPath, &normalPath))
    {
        spdlog::error("[Renderer] glTF load failed: {}", path);
        return false;
    }
    if (md.vertices.empty() || md.indices.empty())
    {
        spdlog::error("[Renderer] glTF mesh empty: {}", path);
        return false;
    }

    // 顶点布局：与 vs_mesh/fs_mesh 对齐（pos/normal/uv）
    bgfx::VertexLayout gltfLayout;
    gltfLayout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    // 顶点/索引上传（注意 const 变量一次性初始化，并给 bgfx::copy 传入 size）
    const uint32_t vsize = static_cast<uint32_t>(md.vertices.size() * sizeof(MeshVertex));
    const bgfx::Memory *vm = bgfx::copy(md.vertices.data(), vsize);
    bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(vm, gltfLayout);

    const uint32_t isize = static_cast<uint32_t>(md.indices.size() * sizeof(uint16_t));
    const bgfx::Memory *im = bgfx::copy(md.indices.data(), isize);
    bgfx::IndexBufferHandle ibh = bgfx::createIndexBuffer(im);

    if (!bgfx::isValid(vbh) || !bgfx::isValid(ibh))
    {
        spdlog::error("[Renderer] create VB/IB failed for {}", path);
        return false;
    }

    // 贴图（没有就用棋盘格兜底）
    bgfx::TextureHandle tex = BGFX_INVALID_HANDLE;
    if (!baseColorPath.empty())
        tex = resCache_.getTexture2D(baseColorPath, /*flipY=*/true);
    if (!bgfx::isValid(tex))
        tex = texDemo_;

    // 缓存到兼容层，renderScene/后续 PBR 用
    LoadedMesh lm;
    lm.vbh = vbh;
    lm.ibh = ibh;
    lm.tex = tex;
    bx::mtxIdentity(lm.model);
    s_loadedMeshes.push_back(lm);

    spdlog::info("[Renderer] addMeshFromGltfToScene OK: vtx={}, idx={}, baseTex='{}'",
                 static_cast<uint32_t>(md.vertices.size()),
                 static_cast<uint32_t>(md.indices.size()),
                 baseColorPath);
    return true;
}

// ========== Scene 渲染（遍历内部缓存与光照Uniform） ==========
void Renderer::renderScene(const Scene &, Camera &)
{
    float view[16], proj[16];
    {
        const bx::Vec3 eye = {0.0f, 1.2f, -3.0f};
        const bx::Vec3 at = {0.0f, 0.7f, 0.0f};
        const bx::Vec3 up = {0.0f, 1.0f, 0.0f};
        bx::mtxLookAt(view, eye, at, up);
        const float aspect = (height_ == 0) ? 1.0f : (float)width_ / (float)height_;
        bx::mtxProj(proj, 60.0f, aspect, 0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);
        bgfx::setViewTransform(viewId_, view, proj);
    }

    bgfx::touch(viewId_);

    static bgfx::UniformHandle u_lightDir = BGFX_INVALID_HANDLE;
    static bgfx::UniformHandle u_pointPosRad = BGFX_INVALID_HANDLE;
    static bgfx::UniformHandle u_pointColInt = BGFX_INVALID_HANDLE;
    static bgfx::UniformHandle s_texColor = BGFX_INVALID_HANDLE;

    if (!bgfx::isValid(u_lightDir))
        u_lightDir = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);
    if (!bgfx::isValid(u_pointPosRad))
        u_pointPosRad = bgfx::createUniform("u_pointPosRad", bgfx::UniformType::Vec4);
    if (!bgfx::isValid(u_pointColInt))
        u_pointColInt = bgfx::createUniform("u_pointColInt", bgfx::UniformType::Vec4);
    if (!bgfx::isValid(s_texColor))
        s_texColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);

    // 上传当前光照
    bgfx::setUniform(u_lightDir, &pbr_.lighting().lightDir_ambient, 1);
    bgfx::setUniform(u_pointPosRad, &pbr_.lighting().pointPos_radius, 1);
    bgfx::setUniform(u_pointColInt, &pbr_.lighting().pointCol_intensity, 1);

    const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                           BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS |
                           BGFX_STATE_CULL_CW;

    for (const auto &m : s_loadedMeshes)
    {
        if (!bgfx::isValid(m.vbh) || !bgfx::isValid(m.ibh) || !bgfx::isValid(programMesh_))
            continue;

        bgfx::setTransform(m.model);
        bgfx::setVertexBuffer(0, m.vbh);
        bgfx::setIndexBuffer(m.ibh);
        if (bgfx::isValid(m.tex))
            bgfx::setTexture(0, s_texColor, m.tex);
        bgfx::setState(state);
        bgfx::submit(viewId_, programMesh_);
    }

    bgfx::frame();
}

// ========== 演示路径：Triangle/Quad ==========
void Renderer::renderFrame(Camera &, uint32_t &outDraws, uint32_t &outTris, float angle)
{
    outDraws = 0;
    outTris = 0;

    float view[16], proj[16];
    {
        const bx::Vec3 eye = {0.0f, 0.0f, -2.5f};
        const bx::Vec3 at = {0.0f, 0.0f, 0.0f};
        const bx::Vec3 up = {0.0f, 1.0f, 0.0f};
        bx::mtxLookAt(view, eye, at, up);
        const float aspect = (height_ == 0) ? 1.0f : (float)width_ / (float)height_;
        bx::mtxProj(proj, 60.0f, aspect, 0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);
        bgfx::setViewTransform(viewId_, view, proj);
    }

    bgfx::touch(viewId_);

    const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                           BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS |
                           BGFX_STATE_CULL_CW;

    switch (drawMode_)
    {
    case DrawMode::Triangle:
        if (bgfx::isValid(vbhTri_) && bgfx::isValid(ibhTri_) && bgfx::isValid(programSimple_))
        {
            float model[16];
            bx::mtxRotateXYZ(model, 0.0f, angle, 0.0f);
            bgfx::setTransform(model);
            bgfx::setVertexBuffer(0, vbhTri_);
            bgfx::setIndexBuffer(ibhTri_);
            bgfx::setState(state);
            bgfx::submit(viewId_, programSimple_);
            outDraws = 1;
            outTris = 1;
        }
        break;

    case DrawMode::Quad:
        if (bgfx::isValid(vbhQuad_) && bgfx::isValid(ibhQuad_))
        {
            const bool useTex = useTexture_ && bgfx::isValid(programTex_);
            const bgfx::ProgramHandle prog = useTex ? programTex_ : programSimple_;
            if (bgfx::isValid(prog))
            {
                float model[16];
                bx::mtxRotateXYZ(model, 0.0f, angle, 0.0f);
                bgfx::setTransform(model);
                bgfx::setVertexBuffer(0, vbhQuad_);
                bgfx::setIndexBuffer(ibhQuad_);

                if (useTex)
                {
                    static bgfx::UniformHandle s_tex = BGFX_INVALID_HANDLE;
                    if (!bgfx::isValid(s_tex))
                        s_tex = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
                    bgfx::setTexture(0, s_tex, texDemo_);
                }

                bgfx::setState(state);
                bgfx::submit(viewId_, prog);
                outDraws = 1;
                outTris = 2;
            }
        }
        break;

    default:
        break;
    }

    bgfx::frame();
}

void Renderer::setShowHelp(bool b) { showHelp_ = b; }
void Renderer::setUseTexture(bool b) { useTexture_ = b; }

bool Renderer::hotReloadShaders()
{
    destroyPipelines();
    bool ok = createPipelines();
    return ok;
}
