#include "App.h"

// 第三方库
#include <spdlog/spdlog.h>
#include <filesystem>
#include <algorithm>

// 我们新增的帧计时器
#include "core/FrameTimer.h"

#ifdef _WIN32
  #define NOMINMAX
  #include <windows.h>
#endif

#include "io/Exporter.h"

// 给 std::filesystem 起别名，写起来更简洁
namespace fs = std::filesystem;

// ===== 文件作用域静态对象（只在本 .cpp 可见） =====
static ke::FrameTimer gTimer;     // 帧计时器（程序启动构造、退出析构）
static double gPrintAccum = 0.0;  // 打印节流器（累计到 1s 打印一次）

// 点光参数（运行时可按键调整）
static bool  sPointOn       = false;
static float sPointPos[3]   = { 2.0f, 2.0f, 2.0f };
static float sPointRadius   = 5.0f;
static float sPointColor[3] = { 1.0f, 1.0f, 1.0f };
static float sPointIntensity= 3.0f;

// ===================================================

bool App::init(int width, int height, const char* title)
{
#ifdef _WIN32
    // Windows 控制台设置为 UTF-8，防止中文日志乱码
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    width_  = width;
    height_ = height;

    // 高 DPI 策略（每监视器 DPI 感知）
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        spdlog::error("SDL_Init failed: {}", SDL_GetError());
        return false;
    }

    window_ = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width_, height_,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!window_)
    {
        spdlog::error("SDL_CreateWindow failed: {}", SDL_GetError());
        return false;
    }

    if (!renderer_.init(window_, width_, height_))
    {
        return false;
    }

    // （可选）调整 FPS 平滑灵敏度：0.05 更稳，0.30 更灵
    gTimer.setSmoothing(0.15);

    // 方向光
    renderer_.setLightDir(-0.5f, -1.0f, -0.2f, 0.15f);

    // 点光默认关闭；先把参数设置好，开关打开后立即生效
    renderer_.setPointLightEnabled(false);
    renderer_.setPointLight(
        sPointPos[0], sPointPos[1], sPointPos[2], sPointRadius,
        sPointColor[0], sPointColor[1], sPointColor[2], sPointIntensity);

    // 相机/渲染器默认状态
    camera_.setViewport(width_, height_);
    camera_.setMode(CameraMode::Perspective);
    renderer_.setShowHelp(true);
    renderer_.setDrawMode(DrawMode::Triangle);
    renderer_.setUseTexture(false);
    renderer_.setDebug(dbgFlags_);

    // 相机初始位姿
    camCtl_.SetPose({ 0.0f, 1.6f, 3.0f }, 0.0f, 0.0f);

    return true;
}

void App::run()
{
    running_ = true;

    while (running_)
    {
        // ---- 输入帧开始 ----
        input_.BeginFrame();

        // ---- 事件处理 ----
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                running_ = false;

            if (e.type == SDL_WINDOWEVENT &&
               (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                e.window.event == SDL_WINDOWEVENT_RESIZED))
            {
                width_  = e.window.data1;
                height_ = e.window.data2;
                renderer_.resize(width_, height_);
                camera_.setViewport(width_, height_);
            }

            if (e.type == SDL_KEYDOWN)
                handleKeyDown(e.key.keysym.sym);

            input_.HandleSDLEvent(e);
        }

        // ---- 计算 dt（秒）+ 每秒打印一次性能信息 ----
        const double dt = gTimer.tick();                 // 本帧耗时（秒）
        angle_ += static_cast<float>(dt);                // demo 自旋转

        gPrintAccum += dt;
        if (gPrintAccum >= 1.0)
        {
            spdlog::info("[Perf] FPS={:.1f}  FrameTime={:.3f} ms  Frames={}  Elapsed={:.1f}s",
                         gTimer.fps(), gTimer.delta() * 1000.0, gTimer.frameCount(), gTimer.elapsed());
            gPrintAccum = 0.0;
        }

        // ---- 相机控制更新（与帧率无关，基于 dt）----
        camCtl_.Update(static_cast<float>(dt));

        // ---- 渲染路径 ----
        if (draw_ == DrawMode::Mesh)
        {
            renderer_.renderScene(scene_, camera_);
        }
        else
        {
            renderer_.renderFrame(camera_, lastDraws_, lastTris_, angle_);
        }

        // ---- 输入帧结束 ----
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
    case SDLK_r:
        renderer_.hotReloadShaders();
        break;

    case SDLK_F1:
        dbgFlags_ ^= BGFX_DEBUG_STATS;
        renderer_.setDebug(dbgFlags_);
        break;

    case SDLK_F2:
        dbgFlags_ ^= BGFX_DEBUG_IFH;
        renderer_.setDebug(dbgFlags_);
        break;

    case SDLK_F3:
        // 保持与你原版一致：直接覆盖为 TEXT 模式
        dbgFlags_ = BGFX_DEBUG_TEXT;
        renderer_.setDebug(dbgFlags_);
        break;

    case SDLK_h:
        showHelp_ = !showHelp_;
        renderer_.setShowHelp(showHelp_);
        break;

    case SDLK_1:
        draw_ = DrawMode::Triangle;
        renderer_.setDrawMode(draw_);
        renderer_.setUseTexture(useTex_);
        break;

    case SDLK_2:
        draw_ = DrawMode::Quad;
        renderer_.setDrawMode(draw_);
        renderer_.setUseTexture(useTex_);
        break;

    case SDLK_3:
        draw_ = DrawMode::Mesh;
        renderer_.setDrawMode(draw_);
        renderer_.setUseTexture(useTex_);
        break;

    case SDLK_o:
        camera_.setMode(CameraMode::Ortho);
        break;

    case SDLK_p:
        camera_.setMode(CameraMode::Perspective);
        break;

    case SDLK_t:
        useTex_ = !useTex_;
        renderer_.setUseTexture(useTex_);
        break;

    case SDLK_l:
    {
        const std::string path = std::string(KE_ASSET_DIR) + "/image.png";
        renderer_.loadTextureFromFile(path);
        break;
    }

    case SDLK_m:
    {
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
        fs::create_directories(outDir, ec);
        fs::path outObj = outDir / "scene.obj";
        scene_.update();
        bool ok = exportSceneToOBJ(scene_, outObj.string(), "scene.mtl");
        if (ok)
            spdlog::info("[App] Exported OBJ to {}", outObj.string());
        else
            spdlog::error("[App] Export OBJ failed!");
        break;
    }

    // 点光控制
    case SDLK_g:
    {
        sPointOn = !sPointOn;
        renderer_.setPointLightEnabled(sPointOn);
        if (sPointOn)
        {
            renderer_.setPointLight(
                sPointPos[0], sPointPos[1], sPointPos[2], sPointRadius,
                sPointColor[0], sPointColor[1], sPointColor[2], sPointIntensity);
            spdlog::info("[Light] Point ON  pos=({:.2f},{:.2f},{:.2f}) R={:.2f} I={:.2f}",
                         sPointPos[0], sPointPos[1], sPointPos[2], sPointRadius, sPointIntensity);
        }
        else
        {
            spdlog::info("[Light] Point OFF");
        }
        break;
    }
    case SDLK_LEFTBRACKET:
    {
        sPointRadius = std::max(0.1f, sPointRadius - 0.5f);
        if (sPointOn)
            renderer_.setPointLight(
                sPointPos[0], sPointPos[1], sPointPos[2], sPointRadius,
                sPointColor[0], sPointColor[1], sPointColor[2], sPointIntensity);
        spdlog::info("[Light] radius = {:.2f}", sPointRadius);
        break;
    }
    case SDLK_RIGHTBRACKET:
    {
        sPointRadius += 0.5f;
        if (sPointOn)
            renderer_.setPointLight(
                sPointPos[0], sPointPos[1], sPointPos[2], sPointRadius,
                sPointColor[0], sPointColor[1], sPointColor[2], sPointIntensity);
        spdlog::info("[Light] radius = {:.2f}", sPointRadius);
        break;
    }
    case SDLK_9:
    {
        sPointIntensity = std::max(0.0f, sPointIntensity - 0.25f);
        if (sPointOn)
            renderer_.setPointLight(
                sPointPos[0], sPointPos[1], sPointPos[2], sPointRadius,
                sPointColor[0], sPointColor[1], sPointColor[2], sPointIntensity);
        spdlog::info("[Light] intensity = {:.2f}", sPointIntensity);
        break;
    }
    case SDLK_0:
    {
        sPointIntensity += 0.25f;
        if (sPointOn)
            renderer_.setPointLight(
                sPointPos[0], sPointPos[1], sPointPos[2], sPointRadius,
                sPointColor[0], sPointColor[1], sPointColor[2], sPointIntensity);
        spdlog::info("[Light] intensity = {:.2f}", sPointIntensity);
        break;
    }

    default:
        break;
    }
}
