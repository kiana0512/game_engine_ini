#pragma once

#include <cstdint>
#include <string>

#include <bgfx/bgfx.h>
#include <bgfx/bgfx.h>

// bgfx 新旧版本兼容：新版本是 vertexlayout.h，旧版本是 vertexdecl.h
// 优先用公共入口头；不同版本内部会拉到 VertexLayout/VertexDecl
#if __has_include(<bgfx/bgfx.h>)
  #include <bgfx/bgfx.h>
  #include <bgfx/platform.h>
#elif __has_include(<bgfx/vertexlayout.h>) || __has_include(<bgfx/vertexdecl.h>)
  // 兼容极老版本拆分头
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

// 只做前置声明，避免循环包含
struct Scene;
class Camera;

// 渲染模式
enum class DrawMode : uint8_t
{
    Simple,
    Tex,
    Mesh,
    Triangle,
    Quad
};

class Renderer
{
public:
    bool init(SDL_Window* window, int width, int height);
    bool loadTextureFromFile(const std::string& path);

    void shutdown();

    // 供 App/Scene 调用的接口（实现放 .cpp）
    void renderScene(const Scene& scene);
    bool addMeshFromGltfToScene(const std::string& path, Scene& scene);

    void resize(int width, int height);
    void setDebug(uint32_t flags);
    void toggleDebug(uint32_t mask);

    void setDrawMode(DrawMode m) { drawMode_ = m; }
    void setUseTexture(bool v)   { useTex_ = v; }
    void setShowHelp(bool v)     { showHelp_ = v; }

    void hotReloadShaders();

    // 每帧渲染；返回统计（HUD）
    void renderFrame(Camera& cam, uint32_t& outDraws, uint32_t& outTris, float angle);
    void buildMeshLayout();
    bool loadMeshFromGltf(const std::string& path);

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

    // Program
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

    // Mesh 管线资源
    bgfx::ProgramHandle      programMesh_   = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle meshVbh_       = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle  meshIbh_       = BGFX_INVALID_HANDLE;
    uint32_t                 meshIndexCount_ = 0;
    bool                     meshLoaded_     = false;
};
