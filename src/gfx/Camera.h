#pragma once
#include <bgfx/bgfx.h>
#include <bx/math.h>

enum class CameraMode { Ortho, Perspective };

class Camera {
public:
    Camera() = default;

    void setMode(CameraMode m) { mode_ = m; dirty_ = true; }
    CameraMode mode() const { return mode_; }

    void setViewport(int w, int h) { width_ = w; height_ = h; dirty_ = true; }

    // 将 view/proj 应用到某个 ViewId
    void apply(bgfx::ViewId viewId) {
        if (dirty_) rebuild();
        bgfx::setViewTransform(viewId, view_, proj_);
    }

private:
    void rebuild() {
        if (mode_ == CameraMode::Ortho) {
            bx::mtxIdentity(view_);
            bx::mtxOrtho(
                proj_, -1.0f, 1.0f, -1.0f, 1.0f,
                0.0f, 100.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);
        } else {
            const bx::Vec3 eye = {0.0f, 0.0f, -2.2f};
            const bx::Vec3 at  = {0.0f, 0.0f,  0.0f};
            const bx::Vec3 up  = {0.0f, 1.0f,  0.0f};
            bx::mtxLookAt(view_, eye, at, up, bx::Handedness::Right);
            bx::mtxProj(
                proj_, 60.0f, float(width_) / float(height_),
                0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);
        }
        dirty_ = false;
    }

private:
    CameraMode mode_ = CameraMode::Ortho;
    int width_ = 1280, height_ = 720;
    bool dirty_ = true;
    float view_[16]{}, proj_[16]{};
};
