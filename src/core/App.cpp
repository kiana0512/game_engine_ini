#include "App.h"
#include <spdlog/spdlog.h>
#include <filesystem>           // ← 必须显式包含它
namespace fs = std::filesystem; // ← 文件作用域 alias，后面直接用 fs::path

#ifdef _WIN32
#include <windows.h>
#endif
#include "io/Exporter.h"
bool App::init(int width, int height, const char *title)
{
#ifdef _WIN32
    // Windows 控制台编码设为 UTF-8，避免中文日志乱码
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    width_ = width;
    height_ = height;

    // 高 DPI 适配（Windows 特有）
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        spdlog::error("SDL_Init failed: {}", SDL_GetError());
        return false;
    }

    // 创建 SDL 窗口（允许高 DPI / 可缩放）
    window_ = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width_, height_, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window_)
    {
        spdlog::error("SDL_CreateWindow failed: {}", SDL_GetError());
        return false;
    }

    // 初始化 bgfx 渲染器
    if (!renderer_.init(window_, width_, height_))
    {
        return false;
    }
    // 初始化 bgfx 渲染器之后：
    renderer_.setLightDir(-0.5f, -1.0f, -0.2f, 0.15f); // 方向从上往下，环境 0.15
    // 初始化相机
    camera_.setViewport(width_, height_);
    camera_.setMode(CameraMode::Perspective); // 默认透视，更符合 WASD 控制
    renderer_.setShowHelp(true);
    renderer_.setDrawMode(DrawMode::Triangle);
    renderer_.setUseTexture(false);
    renderer_.setDebug(dbgFlags_);

    // 初始化相机控制器的起始姿态（位置 + 角度）
    camCtl_.SetPose({0.0f, 1.6f, 3.0f}, 0.0f, 0.0f);

    return true;
}

void App::run()
{
    running_ = true;
    uint32_t lastTicks = SDL_GetTicks();

    while (running_)
    {
        // --- 输入帧开始 ---
        input_.BeginFrame();

        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                running_ = false;

            // 窗口大小变化时，更新渲染器与相机视口
            if (e.type == SDL_WINDOWEVENT &&
                (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                 e.window.event == SDL_WINDOWEVENT_RESIZED))
            {
                width_ = e.window.data1;
                height_ = e.window.data2;
                renderer_.resize(width_, height_);
                camera_.setViewport(width_, height_);
            }

            // 原有：调试/模式切换键
            if (e.type == SDL_KEYDOWN)
                handleKeyDown(e.key.keysym.sym);

            // 新增：把 SDL 事件传给输入系统
            input_.HandleSDLEvent(e);
        }

        // --- 计算时间增量 dt ---
        uint32_t now = SDL_GetTicks();
        float dt = (now - lastTicks) * 0.001f; // 毫秒转秒
        lastTicks = now;
        angle_ += dt;

        // --- 更新相机控制器（处理 WASD 移动 & 鼠标右键拖拽旋转） ---
        camCtl_.Update(dt);

        // --- 渲染 ---
        if (draw_ == DrawMode::Mesh)
        {
            // 使用 Scene 路径（M 键把网格推入 scene_）
            renderer_.renderScene(scene_, camera_);
        }
        else
        {
            // 继续用直绘路径画三角/四边形（含 HUD）
            renderer_.renderFrame(camera_, lastDraws_, lastTris_, angle_);
        }

        // --- 输入帧结束 ---
        input_.EndFrame();
    }
}

void App::shutdown()
{
    renderer_.shutdown();
    if (window_)
    {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

void App::handleKeyDown(SDL_Keycode key)
{
    switch (key)
    {
    // --- 工具与调试键 ---
    case SDLK_r: // 热重载 shader
        renderer_.hotReloadShaders();
        break;

    case SDLK_F1: // 切换统计面板
        dbgFlags_ ^= BGFX_DEBUG_STATS;
        renderer_.setDebug(dbgFlags_);
        break;

    case SDLK_F2: // IFH
        dbgFlags_ ^= BGFX_DEBUG_IFH;
        renderer_.setDebug(dbgFlags_);
        break;

    case SDLK_F3: // 只保留文字
        dbgFlags_ = BGFX_DEBUG_TEXT;
        renderer_.setDebug(dbgFlags_);
        break;

    case SDLK_h: // 显隐帮助
        showHelp_ = !showHelp_;
        renderer_.setShowHelp(showHelp_);
        break;

    // --- 绘制模式 ---
    case SDLK_1: // 三角
        draw_ = DrawMode::Triangle;
        renderer_.setDrawMode(draw_);
        renderer_.setUseTexture(useTex_);
        break;

    case SDLK_2: // 四边形
        draw_ = DrawMode::Quad;
        renderer_.setDrawMode(draw_);
        renderer_.setUseTexture(useTex_);
        break;

    case SDLK_3: // 网格（Mesh 模式）
        draw_ = DrawMode::Mesh;
        renderer_.setDrawMode(draw_);
        renderer_.setUseTexture(useTex_);
        break;

    // --- 相机投影模式 ---
    case SDLK_o:
        camera_.setMode(CameraMode::Ortho);
        break;

    case SDLK_p:
        camera_.setMode(CameraMode::Perspective);
        break;

    // --- 贴图开关 ---
    case SDLK_t:
        useTex_ = !useTex_;
        renderer_.setUseTexture(useTex_);
        break;

    // --- 重新加载纹理 ---
    case SDLK_l:
    {
        const std::string path = std::string(KE_ASSET_DIR) + "/image.png";
        renderer_.loadTextureFromFile(path);
        break;
    }

    // --- 加载 glTF 模型并加入 Scene ---
    case SDLK_m:
    {
        // 修改成你项目的路径：docs/models/model.gltf
        const std::string path = std::string(KE_ASSET_DIR) + "/models/model.gltf";

        bool ok = renderer_.addMeshFromGltfToScene(path, scene_);
        if (ok)
            spdlog::info("[App] Mesh added to Scene from {}", path);
        else
            spdlog::error("[App] Load mesh to Scene failed: {}", path);
        break;
    }
    case SDLK_e:
    {
        fs::path outDir = fs::path(KE_ASSET_DIR) / ".." / "export";

        std::error_code ec;
        fs::create_directories(outDir, ec); // 即使存在也安全

        fs::path outObj = outDir / "scene.obj";

        scene_.update(); // 如需把 SRT 推成 model，这里同步一下

        bool ok = exportSceneToOBJ(scene_, outObj.string(), "scene.mtl");
        if (ok)
            spdlog::info("[App] Exported OBJ to {}", outObj.string());
        else
            spdlog::error("[App] Export OBJ failed!");
        break;
    }

    // --- 兜底 ---
    default:
        break;
    }
}
