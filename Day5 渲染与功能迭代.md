# Day 5 — 渲染与功能迭代（README）

> 目标：在**不破坏现有功能**的前提下，为 Mesh 渲染路径补上**光照（方向光 + 环境光 + 点光）**，理顺 bgfx 初始化与 Uniform 生命周期，并把 Scene 渲染管线跑通。

---

##  今日完成

### 1) 着色器（Mesh 路径）
- `shaders/varying.def.sc`
  - 新增 `v_worldPos : TEXCOORD1`（片元点光计算用）
  - 保留 `v_color0 / v_texcoord0 / v_normal` 与现有顶点输入语义
- `shaders/vs_mesh.sc`
  - 输出 `v_normal / v_texcoord0 / v_worldPos`
  - 位置：`gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0))`
  - 法线：`v_normal = mul(u_model[0], vec4(a_normal, 0.0)).xyz`
- `shaders/fs_mesh.sc`
  - Uniform：
    - `u_lightDir = vec4(dir.xyz, ambient)`
    - `u_pointPosRad = vec4(px, py, pz, radius)`
    - `u_pointColInt = vec4(cr, cg, cb, intensity)`
  - 实现：**Lambert 漫反射 + 环境光 + 点光（平滑衰减）**
- 说明：点光仅在**Mesh 模式**（`vs_mesh/fs_mesh`）生效；`simple/tex` 路径暂未加入光照。

---

### 2) Renderer 初始化与生命周期
- 正确顺序：**SDL 原生句柄** → `bgfx::setPlatformData` → `bgfx::init` → `createUniform/Program/Buffer`
- Uniform 统一管理：
  - 创建：`init()` 成功后
  - 销毁：`shutdown()` 统一销毁，避免重复释放
- `renderScene(...)`：
  - 每帧上传 `u_lightDir / u_pointPosRad / u_pointColInt`
  - 遍历 `Scene.meshes`，按每个 mesh 的 `state/model/VB/IB/texture` 进行 `submit`

---

### 3) 点光控制（可开关）
- Renderer 接口：
  ```cpp
  // 方向光：xyz=方向，w=环境光强度[0,1]
  void setLightDir(float x, float y, float z, float ambient = 0.15f);

  // 点光开关
  void setPointLightEnabled(bool enabled);

  // 点光参数（标量）
  void setPointLight(float px, float py, float pz, float radius,
                     float cr, float cg, float cb, float intensity);

  // 点光参数（数组）
  void setPointLight(const float pos[3], float radius,
                     const float color[3], float intensity);
