# K-ENGINE

一个用 **SDL2 + bgfx** 搭建的学习型渲染/游戏引擎，目标：**结构清晰、可插拔、易拓展**。  
当前默认渲染路径是 **前向 PBR**，并在不破坏现有效果的前提下逐步引入 **Pass 管线** 来瘦身 `Renderer.cpp`。

> 最近一次更新：2025-10-21

---

## 目录结构（精简版）

```
src/
  core/
    App.{h,cpp}
    FrameTimer.{h,cpp}
  gfx/
    camera/                 # 相机与控制器
      Camera.h
    culling/
    lighting/
      Lighting.h
    material/
      PbrMaterial.{h,cpp}
    memory/
      ScopeExit.h
    pipeline/               # Pass 抽象与实例（进行中）
      RenderPass.h
      ClearPass.{h,cpp}
      ForwardPBRPass.{h,cpp}
      # LegacyScenePass.{h,cpp}  ← 可选适配旧逻辑（如需要）
    resource/
      ResourceCache.{h,cpp}
    shaders/
      shader_utils.{h,cpp}
    texture/
      TextureLoader.{h,cpp}
    Renderer.{h,cpp}        # 仍偏“胖”，正在按 Pass 拆分
  io/
    gltf/Exporter.{h,cpp}
shaders/
  vs_pbr.sc  fs_pbr.sc
  vs_mesh.sc fs_mesh.sc
  fs_simple.sc fs_tex.sc
  varying.def.sc
CMakePresets.json
CMakeLists.txt
```

---

## 快速开始

> 需要：CMake ≥ 3.25、Visual Studio 2022（或等效工具链）。  
> 子模块（bgfx/bx/bimg 等）在配置时会拉取或由脚本生成。

```bash
# 克隆仓库并初始化子模块
git clone <your-repo>.git
cd K-ENGINE
git submodule update --init --recursive

# 生成 + 构建（Windows VS2022 预设）
cmake --preset vs2022-x64
cmake --build --preset vs2022-x64 -j
```

运行可执行文件（路径以 `CMakePresets.json` 的构建产物为准）。

---

## 已完成功能（阶段性回顾）

- **基础框架**
  - SDL2 + bgfx 初始化；`App`/`Renderer` 职责分离；`FrameTimer` 统计帧时间。
  - 资源：`ResourceCache` 缓存、`TextureLoader` 加载、`shader_utils` 着色器装载。

- **相机与交互**
  - `Camera`：支持 Ortho / Perspective；可外部注入矩阵（`setView/setProj`）；**已提供** `viewPtr()` / `projPtr()` 只读指针。
  - `CameraController`：自由相机（右键旋转 + WASD/空格/C 平移 + Shift 冲刺）；**已提供** `GetPosition()`。
  - `OrbitController`：轨道相机，便于展示/调试。

- **场景/几何**
  - 网格加载与缓存（VB/IB/AABB）；支持 glTF 导出工具（`io/gltf`）。
  - 视锥裁剪：CPU 侧 AABB × PV 矩阵（支持齐次深度）。

- **渲染（前向 PBR）**
  - `PbrMaterial` + `ForwardPBR` 技术路径：BaseColor、MetallicRoughness、（可接）Normal；sRGB→线性→输出伽马。
  - 灯光：方向光 + 点光参数；能量守恒与 Lambert 校正已修正。
  - 调试：地网、HUD（dbgText）可开关。

- **Pass 架构（引入中）**
  - `pipeline/RenderPass.h` 定义统一接口；已有 `ClearPass`、`ForwardPBRPass` 骨架。
  - `RenderContext` 以**纯数据**（view/proj 指针、camPos、time、drawList 预留）传递，**解耦** Pass 与 Camera/Controller 的成员接口。

---

## 当前渲染路径

### 旧路径（默认，稳定）
在 `Renderer.cpp` 中顺序完成：清屏 → 设置灯光 Uniform → 视锥裁剪 → 调用 `ForwardPBR` 技术提交 → 调试地网/HUD。  
优点：直观、集中；缺点：文件偏“胖”，后续维护成本上升。

### 新路径（进行中：Pass 管线）
- **Pass 是什么？**  
  “在一个 bgfx View 上、以固定顺序做一件渲染事”的**最小调度单元**。  
  Renderer 负责编排：  
  `buildDrawList → sort → for pass in pipeline: pass.prepare(rc); pass.execute(rc);`
- **已有/计划的 Pass**  
  - `ClearPass`：清屏/触发视图。  
  - `ForwardPBRPass`：消费 `DrawList` 做前向 PBR（内部复用现有 `ForwardPBR` “技术层”）。  
  - （可选）`LegacyScenePass`：把旧渲染逻辑适配成一个 Pass，便于平滑过渡。  
  - 未来：`DebugGridPass`、`ImGuiPass`、`ShadowMapPass`、`IBLPass`、`GBufferPass` 等。
- **Renderer 的终态**  
  只做“数据准备 + Pass 调度”，不再直接 `setTexture/submit`。

---

## 相机与控制细节

- **Camera**  
  - `setViewport(w,h)` 更新投影宽高比；`setView()/setProj()` 可由外部注入矩阵；  
  - `viewPtr()/projPtr()` 提供 4×4 列主序矩阵指针（给 bgfx `setViewTransform`/Pass 使用）。

- **CameraController（自由相机）**  
  - 组合键：右键旋转；WASD 平移；空格上升/C 下降；Shift 冲刺；  
  - `SetPose(pos,yaw,pitch)` 设置初始姿态；`GetPosition()` 用于渲染 Uniform（`u_CamPos_Time`）。

- **OrbitController（轨道）**  
  - 围绕目标点旋转与缩放；演示模型时更友好。

---

## PBR 材质（Metallic–Roughness 工作流）

- **纹理采样约定**
  - BaseColor：**sRGB** 取样 → 转线性；  
  - MetallicRoughness：**线性** 取样（R=Metallic，G=Roughness）；  
  - Normal：线性取样（后续将完善切线空间 TBN）。
- **核心 Uniform/Sampler**
  - `u_BaseColor`（`vec4`，xyz 有效）；  
  - `u_MetallicRoughness`（`vec4`，xy 有效）；  
  - `u_CamPos_Time`（`vec4`，xyz=相机位置，w=时间）；  
  - `s_BaseColor` / `s_Normal` / `s_MR`（sampler2D）。
- **光照参数**
  - 方向光：`u_lightDir`（xyz 方向，w 环境强度）；  
  - 点光：`u_pointPosRad`（xyz 位置，w 半径），`u_pointColInt`（rgb 颜色，w 强度）。

---

## 视锥裁剪（CPU）

- 计算 `PV = P * V`；将 AABB 变换到世界后测试。  
- bgfx 能力集中 `homogeneousDepth` 为真时，深度范围为 `[-1,1]`（齐次深度）；否则为 `[0,1]`。  
- 只提交命中的网格，HUD 输出 `draws/tris/culled` 统计。

---

## 近期路线图（Roadmap）

### Sprint A：让 Renderer “有规矩”
- [ ] `Renderer::buildDrawList()`：把 `s_loadedMeshes` 转为 `DrawItem`（VB/IB/Model/Texture/State）。  
- [ ] `Renderer::sortDrawList()`：按 `DrawKey { pipeline(8b), material(24b), depth(32b) }` 排序。  
- [ ] `ForwardPBRPass` 消费 `drawList`，内部复用现有 `ForwardPBR` 实现。  
- [ ] 主循环改为：`ClearPass → ForwardPBRPass`（效果不变，结构清晰）。

### Sprint B：质量提升（不改外部接口）
- [ ] `shaders/include/common.sc`：sRGB 工具、GGX/Smith、Fresnel、TBN/法线解码。  
- [ ] Tangent 生成（MikkTSpace 或导入）；法线贴图完全正确。  
- [ ] Debug 视图（法线/MR/UV/深度）。  
- [ ] 统一 UBO 频率：Per-Frame / Per-Material / Per-Object。

### Sprint C：进阶特性
- [ ] IBL：BRDF LUT、Irradiance、Prefiltered Specular（先加载预烘焙 KTX）。  
- [ ] 阴影：ShadowMap → PCF → CSM。  
- [ ] ImGuiPass：可视化调节灯光/材质/相机参数。

---

## C++ 风格约定

- **RAII**：拥有 GPU 资源的类禁拷贝（`=delete`），析构里 `bgfx::destroy`。  
- **const-correctness**：能 `const` 就 `const`；参数优先 `const&`。  
- **noexcept**：纯工具/不抛异常的成员标 `noexcept`。  
- **位域 + 联合体**：`DrawKey` 64 位排序键，cache 友好。  
- **最小 include**：头文件多用前向声明，`.cpp` 再包含实现头。

---

## 常见问题（Troubleshooting）

- **error C2039: “position” 不是 “Camera” 的成员**  
  不要在 Pass 里直接访问 `Camera` 成员；通过 `RenderContext` 传入 `viewPtr()/projPtr()` 与 `CameraController::GetPosition()`。  
  （本仓库已提供这些接口）

- **颜色不对/发灰**  
  BaseColor 需以 **sRGB** 采样并转线性；MetallicRoughness/Normal 必须线性采样；输出前做伽马/色调映射。

- **裁剪过度/画面消失**  
  确认 `PV = P * V` 顺序；AABB 需先乘 `model` 后再测试；检查 `homogeneousDepth` 分支。

---

## 提交规范

- 示例：`feat(pipeline): add ForwardPBRPass skeleton`  
- PR 请附：动机、前后截图、性能/编译耗时变化。

---

## 变更日志

**2025-10-21**  
- Camera 增加 `viewPtr()/projPtr()`；CameraController 增加 `GetPosition()`。  
- 引入 `pipeline/`：`RenderPass.h`、`ClearPass`、`ForwardPBRPass`（准备接管提交）。  
- `ForwardPBRPass` 改为吃 `RenderContext` 纯数据（不再依赖 Camera 成员名）。  
- 文档重构：本 README 替换旧的 Day4/Day5 文档。

---

## 许可

本项目用于学习与研究；如需商用/二次分发，请自行评估并补充相应许可证。
