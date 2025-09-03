#include "App.h"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

bool App::init(int width, int height, const char *title)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    width_ = width;
    height_ = height;

    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        spdlog::error("SDL_Init failed: {}", SDL_GetError());
        return false;
    }

    window_ = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width_, height_, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window_)
    {
        spdlog::error("SDL_CreateWindow failed: {}", SDL_GetError());
        return false;
    }

    if (!renderer_.init(window_, width_, height_))
    {
        return false;
    }

    camera_.setViewport(width_, height_);
    camera_.setMode(CameraMode::Ortho);
    renderer_.setShowHelp(true);
    renderer_.setDrawMode(DrawMode::Triangle);
    renderer_.setUseTexture(false);
    renderer_.setDebug(dbgFlags_);

    return true;
}

void App::run()
{
    running_ = true;
    uint32_t lastTicks = SDL_GetTicks();
    bgfx::dbgTextPrintf(0, 3, 0x0a, "T: use texture  L: reload docs/image.png");

    while (running_)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                running_ = false;

            if (e.type == SDL_WINDOWEVENT &&
                (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                 e.window.event == SDL_WINDOWEVENT_RESIZED))
            {
                width_ = e.window.data1;
                height_ = e.window.data2;
                renderer_.resize(width_, height_);
                camera_.setViewport(width_, height_);
            }

            if (e.type == SDL_KEYDOWN)
            {
                handleKeyDown(e.key.keysym.sym);
            }
        }

        // 时间推进
        uint32_t now = SDL_GetTicks();
        float dt = (now - lastTicks) * 0.001f;
        lastTicks = now;
        angle_ += dt;

        renderer_.renderFrame(camera_, lastDraws_, lastTris_, angle_);
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
    case SDLK_r: // 热重载两套 shader
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

    case SDLK_o:
        camera_.setMode(CameraMode::Ortho);
        break;
    case SDLK_p:
        camera_.setMode(CameraMode::Perspective);
        break;

    case SDLK_t: // 贴图开关
        useTex_ = !useTex_;
        renderer_.setUseTexture(useTex_);
        break;
    default:
        break;

    case SDLK_l: // L：从磁盘重新加载 docs/image.png
        std::string path = std::string(KE_ASSET_DIR) + "/image.png";
        renderer_.loadTextureFromFile(path);
        break;

    case SDLK_m:
        // M: 从磁盘加载 glTF 模型并切换到网格绘制
        std::string p1 = std::string(KE_ASSET_DIR) + "/models/model.gltf";
        std::string p2 = std::string(KE_ASSET_DIR) + "/models/model.glb";
        bool ok = renderer_.loadMeshFromGltf(p1);
        if (!ok)
            ok = renderer_.loadMeshFromGltf(p2);
        if (ok)
        {
            renderer_.setDrawMode(DrawMode::Mesh); // 你已有 setDrawMode 的话；否则内部切换
            spdlog::info("[App] Mesh mode ON");
        }
        else
        {
            spdlog::error("[App] Load mesh failed (tried model.gltf / model.glb)");
        }
        break;
    }
}
