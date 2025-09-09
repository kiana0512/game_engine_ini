#pragma once

#include <cstdint>
#include <string>

#include <bgfx/bgfx.h>
#include <bgfx/bgfx.h>

// bgfx 新旧版本兼容：优先公共入口
#if __has_include(<bgfx/bgfx.h>)
  #include <bgfx/bgfx.h>
  #include <bgfx/platform.h>
#elif __has_include(<bgfx/vertexlayout.h>) || __has_include(<bgfx/vertexdecl.h>)
  #if __has_include(<bgfx/vertexlayout.h>)
    #include <bgfx/vertexlayout.h>
  #else
    #include <bgfx/vertexdecl.h>
  #endif
  #include <bgfx/bgfx.h>
  #include <bgfx/platform.h>
#else
  #error "Cannot find bgfx public headers. Ensure bgfx 'include' dir is on the include path."
#endif

#include <SDL.h>

// 前置声明
struct Scene;
class Camera;

// 渲染模式
enum class DrawMode : uint8_t
{
    Simple,
    Tex,
    Mesh,      // ← Mesh 模式将使用 Scene 路径
    Triangle,
    Quad
};

class Renderer
{
public:
    bool init(SDL_Window* window, int width, int height);
    bool loadTextureFromFile(const std::string& path);
    void shutdown();

    // Scene 路径：把 glTF 变为 MeshComp 推入 Scene
    bool addMeshFromGltfToScene(const std::string& path, Scene& scene);

    // 主循环里调用：当 drawMode_ == Mesh 时，用 Scene 路径渲染
    void renderScene(const Scene& scene, Camera& cam);

    // 直绘路径（演示三角/四边形 & HUD），非 Mesh 时使用
    void renderFrame(Camera& cam, uint32_t& outDraws, uint32_t& outTris, float angle);

    void resize(int width, int height);
    void setDebug(uint32_t flags);
    void toggleDebug(uint32_t mask);

    void setDrawMode(DrawMode m) { drawMode_ = m; }
    void setUseTexture(bool v)   { useTex_ = v; }
    void setShowHelp(bool v)     { showHelp_ = v; }

    void hotReloadShaders();

    // 可选：把 glTF 直接加载到 Renderer 自己的 VB/IB（直绘 Mesh，用于快速验证）
    bool loadMeshFromGltf(const std::string& path);
    void buildMeshLayout();

private:
    void* nativeWindowHandle(SDL_Window* win);

    // 资源创建/销毁
    bool createPipelines();
    void destroyPipelines();
    bool createGeometry();
    void destroyGeometry();
    bool createTexture();
    void destroyTexture();

    bgfx::TextureHandle createCheckerTexRGBA8(uint16_t w, uint16_t h, uint16_t cell);

private:
    // 基本
    bgfx::ViewId view_ = 1;
    int  width_ = 0, height_ = 0;
    uint32_t dbgFlags_ = 0;

    // Program（基础）
    bgfx::ProgramHandle progColor_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle progTex_   = BGFX_INVALID_HANDLE;

    // 顶点/索引缓冲（基础几何）
    bgfx::VertexLayout       meshLayout_{};
    bgfx::VertexBufferHandle vbhTri_   = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle vbhQuad_  = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle  ibhQuad_  = BGFX_INVALID_HANDLE;

    // 顶点/索引缓冲（纹理）
    bgfx::VertexBufferHandle vbhTriTex_  = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle vbhQuadTex_ = BGFX_INVALID_HANDLE;

    // 纹理与采样器
    bgfx::UniformHandle uSampler_ = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle tex_      = BGFX_INVALID_HANDLE;
    uint32_t samplerFlags_ =
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_W_CLAMP |
        BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT;

    // 开关
    DrawMode drawMode_ = DrawMode::Triangle;
    bool useTex_  = false;
    bool showHelp_ = true;

    // Mesh 管线资源（Scene 路径与直绘 Mesh 共用的 shader）
    bgfx::ProgramHandle      programMesh_    = BGFX_INVALID_HANDLE;

    // —— 仅供“直绘 Mesh”演示使用（Scene 路径不会用到以下三个）
    bgfx::VertexBufferHandle meshVbh_        = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle  meshIbh_        = BGFX_INVALID_HANDLE;
    uint32_t                 meshIndexCount_ = 0;
    bool                     meshLoaded_     = false;
};
