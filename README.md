# K-Engine 开发日志 - Day 3 总结

## 🎯 Day 3 学习目标
1. **Scene/组件层**  
   - 实现最小的场景容器，可以挂载网格、相机。
2. **Material/渲染管线**  
   - 把贴图/光照等状态从 Renderer 内部分离，进入材质管理。
3. **相机控制器**  
   - 支持 WASD 移动、Shift 冲刺、Space/C 上下、右键拖拽旋转。
4. **资源管理**  
   - 最小缓存机制，避免重复加载 Texture/Mesh。
5. **输入映射**  
   - 将 SDL 输入抽象成 Action，便于未来扩展（手柄/配置文件）。

---

## 📂 新增/修改的文件结构
src/
├── core/
│ ├── App.cpp / App.h # 主循环 + 事件处理
├── gfx/
│ ├── Renderer.cpp / Renderer.h # bgfx 渲染器，支持直绘与 Scene
│ ├── Material.cpp / Material.h # 材质封装（状态/贴图）
│ ├── GltfLoader.cpp / GltfLoader.h# glTF 模型加载
├── scene/
│ ├── Scene.cpp / Scene.h # 场景容器 + MeshComp
│ ├── CameraController.cpp / .h # 相机控制器 (WASD + 鼠标)
│ ├── Input.cpp / Input.h # 输入抽象 (ActionMap)

---

## 🖥️ 功能达成情况

### 1. 场景层
- `Scene` 支持多个 `MeshComp`，每个包含：
  - VB/IB
  - 可选 BaseColor 贴图
  - 模型矩阵
- `Renderer::renderScene(scene, camera)` 遍历所有网格绘制。

### 2. 渲染管线
- 三套 shader 管线：
  - `progColor_` → 纯顶点色
  - `progTex_` → 纹理采样
  - `programMesh_` → 网格 + 法线 + UV
- `uSampler_` 统一采样贴图，支持 PNG / 棋盘格 / glTF BaseColor。

### 3. 相机控制器
- WASD 移动，Shift 冲刺，Space 上升，C 下降。
- 鼠标右键拖拽旋转相机。
- 限制 Pitch 防止相机翻转。
- Camera 每帧更新 view/proj 并传入 bgfx。

### 4. 资源管理
- Texture → 自动加载或 fallback 棋盘格。
- Mesh → glTF 加载后生成一次 VB/IB 并交给 bgfx。
- 资源使用统一 layout，避免重复创建。

### 5. 输入映射
- 将 SDL 事件翻译为动作：
  - `MoveForward/Back/Left/Right`
  - `MoveUp/Down`
  - `Sprint`
  - `CameraRotate`
- 结构支持后续扩展（手柄/配置文件）。

---

## ⌨️ 热键一览
- `R` → 热重载 Shader
- `F1/F2/F3` → Debug 面板切换
- `H` → 显隐帮助
- `1 / 2` → 三角形 / 四边形
- `3` → Mesh 模式 (Scene 渲染)
- `O / P` → 正交 / 透视相机
- `T` → 开关贴图
- `L` → 重新加载 `docs/image.png`
- `M` → 加载 `docs/models/model.gltf` 到场景

---

## 🧩 当前引擎具备的能力
- 有了 **场景层** (Scene) 与 **渲染器** (Renderer) 的协作。
- 支持 **多种绘制模式**：直绘三角/四边形、glTF 网格。
- 有了 **相机控制器**，能像 Unity 编辑器一样飞行视角。
- 输入抽象为 Action，后续能接手柄/键位映射。
- 材质/贴图/网格已开始解耦。

---

## 🔜 Day 4 展望
下一步计划：
1. **组件扩展**  
   - 为 `MeshComp` 加 transform（平移/旋转/缩放）。
   - 支持多个相机 / 光源。
2. **光照入门**  
   - 在 `vs_mesh/fs_mesh` 中加入简单 Lambert 漫反射。
3. **资源管理进阶**  
   - 引入资源缓存，避免重复加载贴图/网格。
4. **HUD 扩展**  
   - 在 HUD 中显示 FPS、相机位置等 debug 信息。

---

📌 **总结**  
Day 3 阶段我们完成了：场景容器、材质封装、相机控制、输入抽象、资源加载。  
现在引擎已经进入一个“小型引擎”的雏形，可以加载 glTF 模型并通过相机自由浏览。  
下一步 Day 4 我们将进入 **光照 & 组件扩展**，让画面更有真实感。
