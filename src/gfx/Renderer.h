#pragma once
#include <bgfx/bgfx.h>
#include <SDL.h>
#include <string>

enum class DrawMode
{
    Triangle,
    Quad
};

class Camera;

class Renderer
{
public:
    bool init(SDL_Window *window, int width, int height);
    bool loadTextureFromFile(const std::string &path);

    void shutdown();

    void resize(int width, int height);
    void setDebug(uint32_t flags);
    void toggleDebug(uint32_t mask);

    void setDrawMode(DrawMode m) { drawMode_ = m; }
    void setUseTexture(bool v) { useTex_ = v; }
    void setShowHelp(bool v) { showHelp_ = v; }

    void hotReloadShaders();

    // 每帧渲染一张图；返回统计（用于 HUD 打印）
    void renderFrame(Camera &cam, uint32_t &outDraws, uint32_t &outTris, float angle);

private:
    void *nativeWindowHandle(SDL_Window *win);

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
    int width_ = 0, height_ = 0;
    uint32_t dbgFlags_ = 0;

    // 程序（两套）
    bgfx::ProgramHandle progColor_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle progTex_ = BGFX_INVALID_HANDLE;

    // 顶点/索引缓冲（颜色）
    bgfx::VertexBufferHandle vbhTri_ = BGFX_INVALID_HANDLE;  // 无索引三角
    bgfx::VertexBufferHandle vbhQuad_ = BGFX_INVALID_HANDLE; // 索引四边
    bgfx::IndexBufferHandle ibhQuad_ = BGFX_INVALID_HANDLE;

    // 顶点/索引缓冲（纹理）
    bgfx::VertexBufferHandle vbhTriTex_ = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle vbhQuadTex_ = BGFX_INVALID_HANDLE;

    // 纹理与采样器
    bgfx::UniformHandle uSampler_ = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle tex_ = BGFX_INVALID_HANDLE;
    uint32_t samplerFlags_ = 0;

    // 开关
    DrawMode drawMode_ = DrawMode::Triangle;
    bool useTex_ = false;
    bool showHelp_ = true;

    // 新增：Mesh 管线
    bgfx::ProgramHandle programMesh_ = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle meshVbh_ = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle meshIbh_ = BGFX_INVALID_HANDLE;
    uint32_t meshIndexCount_ = 0;
    bool meshLoaded_ = false;

    bool loadMeshFromGltf(const std::string &path);
};
