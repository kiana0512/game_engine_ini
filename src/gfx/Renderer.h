#pragma once
//
// Renderer.h — 渲染器头文件（去重/修正版）
// 关键修复：
//  1) 去掉重复 include / 重复成员定义（尤其是 bgfx 和 lightDir_/uLightDir_ 重复问题）。
//  2) 补回私有成员：drawMode_ / useTex_ / showHelp_，修复未声明错误。
//  3) 统一句柄默认值为 BGFX_INVALID_HANDLE。
//  4) 点光接口支持标量与数组两种写法，兼容现有 App.cpp 调用。

#include <cstdint>
#include <string>
#include <unordered_map>
#include <SDL.h>

// ===== bgfx 头文件（新旧版本兼容）=====
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

// 前置声明，避免环包含
struct Scene;
class Camera;

// 渲染模式（保持你原有的语义）
enum class DrawMode : uint8_t
{
  Simple,
  Tex,
  Mesh,     // Mesh 模式：使用 Scene 路径
  Triangle,
  Quad
};

class Renderer
{
public:
  // ====== 光照：方向光 ======
  // 设置方向光（xyz）与环境强度（w，0~1）
  inline void setLightDir(float x, float y, float z, float ambient = 0.15f)
  {
    lightDir_[0] = x;
    lightDir_[1] = y;
    lightDir_[2] = z;
    lightDir_[3] = ambient;
  }

  // ====== 光照：点光（开关 + 参数）======
  // 开/关点光（关闭时内部向着色器传 0，保持旧效果）
  void setPointLightEnabled(bool enabled);

  // 标量版（推荐）：位置/半径/颜色/强度
  void setPointLight(float px, float py, float pz, float radius,
                     float cr, float cg, float cb, float intensity);

  // 数组版（兼容 App.cpp 早期调用）
  inline void setPointLight(const float pos[3], float radius,
                            const float color[3], float intensity)
  {
    setPointLight(pos[0], pos[1], pos[2], radius,
                  color[0], color[1], color[2], intensity);
  }

  // ====== 生命周期与资源 ======
  bool init(SDL_Window* window, int width, int height);
  void shutdown();
  void resize(int width, int height);

  // ====== 调试与模式 ======
  void setDebug(uint32_t flags);
  void toggleDebug(uint32_t mask);
  inline void setDrawMode(DrawMode m) { drawMode_ = m; }
  inline void setUseTexture(bool v)   { useTex_   = v; }
  inline void setShowHelp(bool v)     { showHelp_ = v; }
  void hotReloadShaders();

  // ====== 演示直绘（非 Scene 路径）======
  bool loadTextureFromFile(const std::string& path);
  bool loadMeshFromGltf(const std::string& path); // 直绘 Mesh：把 glTF 直接加载成 VB/IB
  void buildMeshLayout();
  void renderFrame(Camera& cam, uint32_t& outDraws, uint32_t& outTris, float angle);

  // ====== Scene 路径（Mesh 模式使用）======
  bool addMeshFromGltfToScene(const std::string& path, Scene& scene);
  void renderScene(const Scene& scene, Camera& cam);

private:
  // ====== 与 fs_mesh.sc 对齐的 Uniform 句柄 ======
  // u_lightDir : vec4(xyz=direction, w=ambient)
  bgfx::UniformHandle uLightDir_     = BGFX_INVALID_HANDLE;
  // u_pointPosRad : vec4(xyz=pos, w=radius)
  bgfx::UniformHandle uPointPosRad_  = BGFX_INVALID_HANDLE;
  // u_pointColInt : vec4(xyz=color, w=intensity)
  bgfx::UniformHandle uPointColInt_  = BGFX_INVALID_HANDLE;

  // ====== 光照运行时参数 ======
  // 方向光（默认从上往下，环境 0.15）
  float lightDir_[4]    = { -0.5f, -1.0f, -0.2f, 0.15f };
  // 点光（默认关闭：半径/强度=0）
  float pointPosRad_[4] = {  0.0f,  0.0f,  0.0f, 0.0f };
  float pointColInt_[4] = {  0.0f,  0.0f,  0.0f, 0.0f };
  bool  pointEnabled_   = false;

  // ====== 纹理缓存与采样 ======
  std::unordered_map<std::string, bgfx::TextureHandle> texCache_;
  bgfx::TextureHandle getOrLoadTexture(const std::string& path, bool srgb, uint32_t samplerFlags);
  bgfx::UniformHandle uSampler_      = BGFX_INVALID_HANDLE;
  bgfx::TextureHandle tex_           = BGFX_INVALID_HANDLE;
  uint32_t samplerFlags_ =
      BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_W_CLAMP |
      BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT;

  // ====== 程序与几何（基础/直绘）======
  bgfx::ProgramHandle progColor_     = BGFX_INVALID_HANDLE;
  bgfx::ProgramHandle progTex_       = BGFX_INVALID_HANDLE;
  bgfx::VertexLayout  meshLayout_    {};
  bgfx::VertexBufferHandle vbhTri_   = BGFX_INVALID_HANDLE;
  bgfx::VertexBufferHandle vbhQuad_  = BGFX_INVALID_HANDLE;
  bgfx::IndexBufferHandle  ibhQuad_  = BGFX_INVALID_HANDLE;
  bgfx::VertexBufferHandle vbhTriTex_= BGFX_INVALID_HANDLE;
  bgfx::VertexBufferHandle vbhQuadTex_= BGFX_INVALID_HANDLE;

  // ====== Mesh 管线（Scene 路径与直绘 Mesh 共用 shader）======
  bgfx::ProgramHandle programMesh_   = BGFX_INVALID_HANDLE;

  // 仅用于“直绘 Mesh”演示（Scene 路径不用以下三个）
  bgfx::VertexBufferHandle meshVbh_  = BGFX_INVALID_HANDLE;
  bgfx::IndexBufferHandle  meshIbh_  = BGFX_INVALID_HANDLE;
  uint32_t                 meshIndexCount_ = 0;
  bool                     meshLoaded_     = false;

  // ====== 视图与窗口 ======
  bgfx::ViewId  view_   = 1;
  int           width_  = 0;
  int           height_ = 0;
  uint32_t      dbgFlags_ = 0;

  // ====== UI/模式开关（这三个是你 .cpp 里在用的）======
  DrawMode drawMode_ = DrawMode::Triangle;
  bool     useTex_   = false;
  bool     showHelp_ = true;

  // ====== 平台句柄、资源创建/销毁 ======
  void* nativeWindowHandle(SDL_Window* win);
  bool  createPipelines();
  void  destroyPipelines();
  bool  createGeometry();
  void  destroyGeometry();
  bool  createTexture();
  void  destroyTexture();
  bgfx::TextureHandle createCheckerTexRGBA8(uint16_t w, uint16_t h, uint16_t cell);
};
