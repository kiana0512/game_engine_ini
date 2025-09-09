#pragma once
#include <SDL.h>
#include <string>
#include <cstdint>
#include "gfx/Renderer.h"
#include "gfx/Camera.h"
#include "scene/Scene.h" 

// 新增：输入与相机控制
#include "scene/Input.h"
#include "scene/CameraController.h"

class App {
public:
    bool init(int width, int height, const char* title);
    void run();
    void shutdown();

private:
    void handleKeyDown(SDL_Keycode key);

private:
    SDL_Window* window_ = nullptr;
    int width_ = 0, height_ = 0;

    Renderer renderer_;
    Camera   camera_;
    Scene    scene_; 

    // 新增：输入系统与相机控制器
    Input input_;
    CameraController camCtl_{ &camera_, &input_ };

    // 状态
    bool running_   = false;
    bool showHelp_  = true;
    DrawMode draw_  = DrawMode::Triangle;
    bool useTex_    = false;

    // 调试
    uint32_t dbgFlags_ = BGFX_DEBUG_TEXT;

    // 帧数据
    float angle_ = 0.0f;
    uint32_t lastDraws_ = 0, lastTris_ = 0;
};
