#include "App.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

static bool  sPointOn        = false;
static float sPointPos[3]    = { 2.0f, 2.0f, 2.0f };
static float sPointRadius    = 5.0f;
static float sPointColor[3]  = { 1.0f, 1.0f, 1.0f };
static float sPointIntensity = 3.0f;

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#include "io/Exporter.h"

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

    renderer_.setLightDir(-0.5f, -1.0f, -0.2f, 0.15f);

    // 点光默认关闭；预先设置一次参数，开关后立即生效
    renderer_.setPointLightEnabled(false);
    renderer_.setPointLight(
        sPointPos[0], sPointPos[1], sPointPos[2], sPointRadius,
        sPointColor[0], sPointColor[1], sPointColor[2], sPointIntensity);

    camera_.setViewport(width_, height_);
    camera_.setMode(CameraMode::Perspective);
    renderer_.setShowHelp(true);
    renderer_.setDrawMode(DrawMode::Triangle);
    renderer_.setUseTexture(false);
    renderer_.setDebug(dbgFlags_);

    camCtl_.SetPose({0.0f, 1.6f, 3.0f}, 0.0f, 0.0f);
    return true;
}

void App::run()
{
    running_ = true;
    uint32_t lastTicks = SDL_GetTicks();

    while (running_)
    {
        input_.BeginFrame();

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
                handleKeyDown(e.key.keysym.sym);

            input_.HandleSDLEvent(e);
        }

        uint32_t now = SDL_GetTicks();
        float dt = (now - lastTicks) * 0.001f;
        lastTicks = now;
        angle_ += dt;

        camCtl_.Update(dt);

        if (draw_ == DrawMode::Mesh)
        {
            renderer_.renderScene(scene_, camera_);
        }
        else
        {
            renderer_.renderFrame(camera_, lastDraws_, lastTris_, angle_);
        }

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
