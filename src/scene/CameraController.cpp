#include "CameraController.h"
#include <SDL.h>        // 若已在头文件包含，可去掉本行
#include <bx/math.h>
#include <cmath>        // std::sin / std::cos

// 将 float[3] 包装成 bx::Vec3，再调用 bgfx 的 mtxLookAt（修复 C2664）
static inline void mtxLookAt_arr(float* out, const float* eye, const float* at)
{
    const bx::Vec3 eyeV  = { eye[0],  eye[1],  eye[2]  };
    const bx::Vec3 atV   = { at[0],   at[1],   at[2]   };
    const bx::Vec3 upV   = { 0.0f, 1.0f, 0.0f };
    bx::mtxLookAt(out, eyeV, atV, upV, bx::Handedness::Right);
}

static inline void mtxProj_persp(float* out, float fovYRadians, float aspect)
{
    bx::mtxProj(out, fovYRadians, aspect, 0.1f, 100.0f, true, bx::Handedness::Right);
}

void CameraController::init(CameraComp* cam, int w, int h)
{
    cam_ = cam; vpw_ = w; vph_ = h;
    rebuildViewProj();
}

void CameraController::onEvent(const SDL_Event& e)
{
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) {
        rotating_ = true; lastX_ = e.button.x; lastY_ = e.button.y;
    }
    if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT) {
        rotating_ = false;
    }
    if (e.type == SDL_MOUSEMOTION && rotating_) {
        const int dx = e.motion.x - lastX_;
        const int dy = e.motion.y - lastY_;
        lastX_ = e.motion.x; lastY_ = e.motion.y;
        yaw_   += dx * 0.01f;
        pitch_ += dy * 0.01f;
        pitch_  = bx::clamp(pitch_, -1.3f, 1.3f);
        rebuildViewProj();
    }
    if (e.type == SDL_MOUSEWHEEL) {
        dist_ *= (e.wheel.y > 0 ? 0.9f : 1.1f);
        dist_  = bx::clamp(dist_, 0.5f, 20.0f);
        rebuildViewProj();
    }
}

void CameraController::update(float /*dt*/) {}

void CameraController::rebuildViewProj()
{
    if (!cam_) return;

    // 观测目标在原点
    float at[3] = { 0.0f, 0.0f, 0.0f };

    // 轨道相机的眼睛位置（右手系）
    const float cx = std::cos(pitch_);
    float eye[3] = {
        dist_ * cx * std::cos(yaw_),
        dist_ * std::sin(pitch_),
        dist_ * cx * std::sin(yaw_)
    };

    mtxLookAt_arr(cam_->view, eye, at);

    const float aspect = (vph_ != 0) ? (float)vpw_ / (float)vph_ : 1.0f;
    mtxProj_persp(cam_->proj, bx::toRad(60.0f), aspect);
}
