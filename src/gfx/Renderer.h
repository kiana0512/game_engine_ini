#pragma once

// Renderer.h — 瘦身版渲染器（模块化）
// 作用：只负责窗口/bgfx 的生命周期、旧路径的少量演示 API，
//      以及把“真正的渲染”委托给独立管线（ForwardPBR）。
//
// 名称速记：
// - DrawMode：演示路径怎么画（Triangle/Quad/…）；Mesh/Scene 仍可保留接口。
// - createPipelines：加载示例用着色器程序(vs_simple/fs_simple 等)。
// - ForwardPBR：我们 Day6 新增的 PBR 前向管线（独立模块）。
// - PbrMaterialManager：PBR 材质的创建/缓存。
// - Lighting 统一放在 ForwardPBR::lighting() 里管理（本类仅提供转发接口）。
//
// 注：为了兼容你现有工程，保留了旧接口签名；内部实现尽量最小化。

#include <cstdint>
#include <string>
#include <SDL.h>

#if __has_include(<bgfx/bgfx.h>)
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#else
#error "bgfx headers not found"
#endif

// 前置声明，避免头文件相互包含
struct Scene;
class Camera;

#include "gfx/pipeline/ForwardPBR.h"
#include "gfx/material/PbrMaterial.h"
#include "resource/ResourceCache.h"

// 渲染模式（演示路径用）
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
  // ===== 生命周期 =====
  bool init(SDL_Window *window, int width, int height);
  void shutdown();
  void resize(int width, int height);
  void setShowHelp(bool b);
  void setUseTexture(bool b);
  bool hotReloadShaders();

  // ===== 调试/视图 =====
  void setDebug(uint32_t flags);
  void setViewId(uint8_t id) { viewId_ = id; }
  uint8_t viewId() const { return viewId_; }

  // ===== 灯光与曝光（转发给 ForwardPBR::lighting）=====
  void setLightDir(float x, float y, float z, float ambient = 0.15f);
  void setPointLightEnabled(bool enabled);
  void setPointLight(float px, float py, float pz, float radius,
                     float cr, float cg, float cb, float intensity);
  void setPointLight(const float pos[3], float radius,
                     const float color[3], float intensity);
  void setViewPos(float x, float y, float z);
  void setExposure(float e);

  // ===== 材质 / PBR 绘制 =====
  PbrMatHandle createPbrMaterial(const PbrMaterialDesc &d);
  void drawMeshPBR(const float *modelMtx /*column-major 4x4*/,
                   bgfx::VertexBufferHandle vbh,
                   bgfx::IndexBufferHandle ibh,
                   PbrMatHandle h,
                   uint8_t viewId = 0);

  // ===== 旧演示路径（保留最小能力，方便对照）=====
  void setDrawMode(DrawMode m) { drawMode_ = m; }
  DrawMode drawMode() const { return drawMode_; }
  bool loadTextureFromFile(const std::string &path);
  bool loadMeshFromGltf(const std::string &path); // 若未实现，返回 false
  void buildMeshLayout();                         // 顶点声明（演示路径）
  void renderFrame(Camera &cam, uint32_t &outDraws, uint32_t &outTris, float angle);
  bool addMeshFromGltfToScene(const std::string &path, Scene &scene); // 占位
  void renderScene(const Scene &scene, Camera &cam);                  // 占位

private:
  bool showHelp_ = false;
  bool useTexture_ = true;
  // ===== 平台/资源 =====
  void *nativeWindowHandle(SDL_Window *win);
  bool createPipelines();
  void destroyPipelines();
  bool createGeometry();
  void destroyGeometry();
  bool createTexture();
  void destroyTexture();
  bgfx::TextureHandle createCheckerTexRGBA8(uint16_t w, uint16_t h, uint16_t cell);

private:
  // 基本状态
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint8_t viewId_ = 0;

  // 旧演示路径：程序/布局/几何/纹理
  bgfx::ProgramHandle programSimple_ = BGFX_INVALID_HANDLE;
  bgfx::ProgramHandle programTex_ = BGFX_INVALID_HANDLE;
  bgfx::ProgramHandle programMesh_ = BGFX_INVALID_HANDLE;

  bgfx::VertexLayout layout_; // 顶点布局（演示路径）
  bgfx::VertexBufferHandle vbhTri_ = BGFX_INVALID_HANDLE;
  bgfx::IndexBufferHandle ibhTri_ = BGFX_INVALID_HANDLE;
  bgfx::VertexBufferHandle vbhQuad_ = BGFX_INVALID_HANDLE;
  bgfx::IndexBufferHandle ibhQuad_ = BGFX_INVALID_HANDLE;

  bgfx::TextureHandle texDemo_ = BGFX_INVALID_HANDLE;

  DrawMode drawMode_ = DrawMode::Triangle;

  // PBR 管线与材质池
  ForwardPBR pbr_;
  PbrMaterialManager matMgr_;

  // 简易资源缓存（纹理等）
  ResourceCache resCache_;
};
