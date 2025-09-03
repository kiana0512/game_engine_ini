# K-Engine — Day 2

一个用 **SDL2 + bgfx + CMake** 起步的学习型小引擎。  
**Day 2 完成内容：**

- 渲染管线抽拆：`core/App`、`gfx/Renderer`、`gfx/Camera`
- 顶点/索引绘制：三角形与四边形（可切换）
- 纹理采样：加载 `docs/image.png`，失败回落棋盘
- **统一 varying 方案**：所有 shader 共享 `shaders/varying_all.def.sc`
- Shader 热重载（按 **R**）
- 调试 HUD 与开关（F1/F2/F3/…）
- 预留 glTF 网格通道（`Renderer::loadMeshFromGltf`）

---

## 0) 依赖 & 环境

- Windows + VS 2022
- CMake ≥ 3.25
- vcpkg（通过 `CMakePresets.json` 自动接入）
- 包：`SDL2`、`spdlog`、`fmt`
- 子模块：`extern/bgfx.cmake`（包含 `bx` / `bimg`）

首次克隆后初始化子模块：
```bash
git submodule update --init --recursive
1) 目录结构
K-Engine/
├─ extern/                # 第三方（包含 bgfx.cmake 子模块）
├─ shaders/               # 所有 shader 与统一 varying
│  ├─ varying_all.def.sc  # ✅ Day 2：统一 varying
│  ├─ vs_simple.sc / fs_simple.sc
│  ├─ vs_tex.sc    / fs_tex.sc
│  └─ vs_mesh.sc   / fs_mesh.sc
├─ src/
│  ├─ core/
│  │  ├─ App.h / App.cpp         # 应用壳，消息循环/热重载
│  └─ gfx/
│     ├─ Camera.h / Camera.cpp   # 正交/透视，相机矩阵
│     ├─ Renderer.h / Renderer.cpp
│     ├─ shader_utils.h/.cpp     # 读取 .bin program
│     ├─ TextureLoader.h/.cpp    # 从文件加载纹理（stb_image）
│     └─ GltfLoader.h/.cpp       # glTF 解析（tinygltf）（预留）
├─ docs/
│  └─ image.png           # 示例贴图
├─ CMakeLists.txt
└─ CMakePresets.json
2) 构建与运行
# 生成
cmake --preset vs2022-x64

# 编译（含自动编译 shaders 并拷贝到 exe 同级 /shaders/dx11）
cmake --build out/build/vs2022-x64 --config RelWithDebInfo

# 运行
out/build/vs2022-x64/RelWithDebInfo/KEngine.exe
若遇到 shader 相关莫名错误，建议 清空构建目录后全量重建：
rmdir /s /q out\build\vs2022-x64
cmake --preset vs2022-x64
cmake --build out/build/vs2022-x64 --config RelWithDebInfo
3) 运行时快捷键

R：热重载 shader（从 shaders/dx11/*.bin）

F1 / F2 / F3：切换 bgfx 调试面板（统计 / IFH / 仅文字）

H：显示/隐藏帮助文字

1 / 2：三角形（非索引）/ 四边形（索引）

O / P：正交 / 透视相机

4) Day 2 关键：统一 varying

为避免 v_texcoord0、v_color0 等“未声明”问题，所有 VS/FS 共用一份 varying：

shaders/varying_all.def.sc
// 统一的 varying，所有 VS/FS 共用；未使用的会被 shaderc 裁剪
vec4 v_color0    : COLOR0;     // simple 用
vec2 v_texcoord0 : TEXCOORD0;  // tex、mesh 用
vec3 v_normal    : NORMAL;     // mesh（光照/NPR）用
Shader 示例

shaders/vs_simple.sc
$input  a_position, a_color0
$output v_color0
#include <bgfx_shader.sh>

uniform mat4 u_modelViewProj;

void main() {
  gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
  v_color0    = a_color0;
}
shaders/fs_simple.sc

$input v_color0
#include <bgfx_shader.sh>

void main() { gl_FragColor = v_color0; }


shaders/vs_mesh.sc

$input  a_position, a_normal, a_texcoord0
$output v_texcoord0, v_normal
#include <bgfx_shader.sh>

uniform mat4 u_modelViewProj;

void main() {
  gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
  v_texcoord0 = a_texcoord0;
  v_normal    = a_normal;
}


shaders/fs_mesh.sc

$input v_texcoord0, v_normal
#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

void main() {
  vec4 base = texture2D(s_texColor, v_texcoord0);
  gl_FragColor = base;
}


CMake（片段） —— 确保所有 shader 都指定了同一份 varying：

# 使用统一的 varying_all.def.sc
bgfx_shader_multi_with_varying(VS_SIMPLE_BINS vs_simple v ${SHADER_DIR}/varying_all.def.sc)
bgfx_shader_multi_with_varying(FS_SIMPLE_BINS fs_simple f ${SHADER_DIR}/varying_all.def.sc)
bgfx_shader_multi_with_varying(VS_TEX_BINS    vs_tex    v ${SHADER_DIR}/varying_all.def.sc)
bgfx_shader_multi_with_varying(FS_TEX_BINS    fs_tex    f ${SHADER_DIR}/varying_all.def.sc)
bgfx_shader_multi_with_varying(VS_MESH_BINS   vs_mesh   v ${SHADER_DIR}/varying_all.def.sc)
bgfx_shader_multi_with_varying(FS_MESH_BINS   fs_mesh   f ${SHADER_DIR}/varying_all.def.sc)


务必删除 旧的 bgfx_shader_multi(vs_mesh ...) / (fs_mesh ...)，避免继续引用默认 varying.def.sc。

5) 常见问题
Q1：undeclared identifier 'v_texcoord0' / 'v_color0'

所有 shader 统一使用 shaders/varying_all.def.sc

CMake 编译命令里都要传 --varyingdef varying_all.def.sc

清理构建目录后重新生成

Q2：只看到统计面板没有画面

面板会遮挡画面，改用 F3 仅文字 或默认 BGFX_DEBUG_TEXT

左上角 HUD 用 dbgTextPrintf() 显示上一帧统计

Q3：贴图不显示

程序尝试加载 docs/image.png；失败则用棋盘纹理

控制台查看 spdlog 输出（UTF-8）

6) 开发提示

最小可运行优先：每加一项功能都保证可运行、可观察（快捷键/日志/HUD）

资源路径通过宏注入：

KE_SHADER_DIR：构建目录的 shaders/

KE_ASSET_DIR：源库 docs/（示例图片）

Shader 改动 → 按 R 热重载，无需重启程序

7) Day 3 路线（预告）

真实绘制 glTF 网格（位置/法线/UV → VBO/IBO）

统一网格顶点布局（与 varying_all.def.sc 对齐）

sRGB、采样器、重复/镜像、双线性

简单 NPR/三渲二：法线外扩描边 + Toon 分段

Blender → glTF 导出建议（法线、切线、单位、Y 轴向上）

8) 许可证

仅用于学习与交流；第三方库遵循各自许可证。