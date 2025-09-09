# K-Engine — Day 4 总结

## 今日目标
- **导出功能**：在已有场景/网格系统的基础上，支持将 `Scene` 导出为 `.obj + .mtl` 文件，便于在 **Blender / DCC 工具** 中进一步渲染和调试。
- **修复与增强**：
  - 统一 `MeshComp`，加入 `cpuVertices / cpuIndices / baseColorPath`，支持 CPU 侧缓存。
  - 完成 `Renderer::addMeshFromGltfToScene`，把加载的 glTF 顶点和索引同时推送到 GPU (VB/IB) 和 CPU 缓存。
  - 完成 `Exporter.cpp`，支持遍历场景，生成标准 OBJ/MTL 文件。
  - 在 `App::handleKeyDown` 新增快捷键 `E` → 导出当前场景到 `export/scene.obj`。

---

## 今日完成的主要内容

### 1. **Scene/MeshComp 扩展**
- `MeshComp` 不再嵌套同名结构。
- 统一字段：`cpuVertices`, `cpuIndices`, `baseColorPath`。
- 保持 Day3 的 SRT（position/rotation/scale）+ `updateModel()`，渲染功能不受影响。

### 2. **Renderer 扩展**
- `addMeshFromGltfToScene`：
  - 调用 `loadGltfMesh()` 拿到 `MeshData`。
  - 创建 VB/IB 提交给 GPU。
  - 同时将 `vertices / indices` 填充到 `MeshComp::cpuVertices / cpuIndices`。
  - 保存纹理路径到 `baseColorPath`。

### 3. **Exporter.cpp**
- 遍历 `Scene::meshes`。
- 写入 OBJ：
  - `v / vt / vn` → 顶点、UV、法线。
  - `f` → 三角面索引（OBJ 索引从 1 开始）。
- 写入 MTL：
  - 基础漫反射白色，支持贴图路径。
- Blender 友好：UV 的 V 坐标翻转。

### 4. **App.cpp**
- 新增 `case SDLK_e:` → 导出场景。
- 使用 `std::filesystem` 创建 `export/` 目录。
- 输出路径：`<ProjectRoot>/export/scene.obj` + `scene.mtl`。

---

## 最终成果

- **键盘交互**：
  - `1 / 2 / 3` → 三角/Quad/Mesh 模式。
  - `M` → 加载 glTF 模型。
  - `E` → 导出 OBJ。
- **导出文件**：
  - `export/scene.obj`
  - `export/scene.mtl`
- 可直接在 Blender 中打开，进行材质、灯光、动画的进一步模拟。

---

## 下一步计划 (Day5 预告)
- 改进 **资源系统**：在 `Scene` 中维护统一的 `AssetManager`。
- 支持多模型实例、多材质切换。
- 初步接入简单的光源和阴影。

---
