#pragma once
#include "Scene.h"
#include <SDL.h>

class CameraController {
public:
    void init(CameraComp* cam, int width, int height);
    void onEvent(const SDL_Event& e);
    void update(float dt);
private:
    CameraComp* cam_ = nullptr;
    float yaw_ = 0.0f, pitch_ = 0.0f, dist_ = 3.0f;
    int vpw_=1280,vph_=720;
    bool rotating_ = false;
    int lastX_=0,lastY_=0;
    void rebuildViewProj();
};
