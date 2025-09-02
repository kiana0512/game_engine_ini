# K-Engine (learning)

一个用 **CMake + SDL2 + bgfx** 起步的学习型游戏引擎项目。当前 Day 1 目标：跑通渲染初始化、编译/加载 bgfx 着色器、画出基本三角形。

![screenshot](docs/screenshot.png)

## 功能概览（Day 1）
- Windows + D3D11 后端（bgfx 自动选择）
- CMake 自定义规则编译 `shaders/*.sc` 并复制到可执行目录
- 运行时加载 `vs_simple.bin / fs_simple.bin`
- 简单位移/旋转示例，按 **R** 热重载 shader

---

## 依赖
- Windows 10/11，Visual Studio 2022（MSVC）
- CMake >= 3.25
- vcpkg（`SDL2`, `spdlog`, `fmt` 通过 vcpkg 引入）
- 子模块：`extern/bgfx.cmake`（包含 `bgfx/bx/bimg`）

> 克隆：
> ```bash
> git clone --recursive https://github.com/kiana0512/game_engine_ini.git
> ```

---

## 构建

### 方式一：使用 CMake Presets（推荐）
项目已提供 `CMakePresets.json`。

```bash
# 生成
cmake --preset vs2022-x64

# 编译（Debug 或 RelWithDebInfo 等）
cmake --build --preset vs2022-x64
