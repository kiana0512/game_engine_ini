#pragma once
#include <bgfx/bgfx.h>
#include <bx/math.h>

// 简单相机：支持正交/透视两种模式；允许外部注入 view/proj 矩阵
enum class CameraMode { Ortho, Perspective };

class Camera {
public:
    Camera() = default;

    // 切换相机模式（只影响投影矩阵）
    void setMode(CameraMode m) {
        mode_ = m;
        useExternalProj_ = false;   // 下次 apply() 重新按模式生成投影
    }
    CameraMode mode() const { return mode_; }

    // 更新视口（只影响投影矩阵的宽高比）
    void setViewport(int w, int h) {
        width_ = w; height_ = h;
        useExternalProj_ = false;   // 需要重建投影
    }

    // 由外部（例如 CameraController）直接设置视图矩阵
    void setView(const float* viewMtx) {
        bx::memCopy(view_, viewMtx, sizeof(float)*16);
        useExternalView_ = true;    // 后续 apply() 不会重建 view
    }

    // 可选：外部直接设置投影矩阵
    void setProj(const float* projMtx) {
        bx::memCopy(proj_, projMtx, sizeof(float)*16);
        useExternalProj_ = true;    // 后续 apply() 不会重建 proj
    }

    // 把 view/proj 应用到指定的 ViewId
    void apply(bgfx::ViewId viewId) {
        // 仅对需要的部分重建，避免覆盖外部注入的矩阵
        if (!useExternalView_)  rebuildView_();
        if (!useExternalProj_)  rebuildProj_();
        bgfx::setViewTransform(viewId, view_, proj_);
    }

private:
    // 只重建视图矩阵（默认固定在世界原点看向 +Z，可按需修改）
    void rebuildView_() {
        // 缺省视角：从 (0,0,-2.2) 看向原点，右手系
        const bx::Vec3 eye = {0.0f, 0.0f, -2.2f};
        const bx::Vec3 at  = {0.0f, 0.0f,  0.0f};
        const bx::Vec3 up  = {0.0f, 1.0f,  0.0f};
        bx::mtxLookAt(view_, eye, at, up, bx::Handedness::Right);
        // 注意：这里不把 useExternalView_ 置 true，保持“默认由相机内部维护”的语义
    }

    // 只重建投影矩阵（根据模式与视口）
    void rebuildProj_() {
        if (mode_ == CameraMode::Ortho) {
            // NDC 区域 [-1,1] 的单位正交；近远裁剪按需可改
            bx::mtxOrtho(
                proj_, -1.0f, 1.0f, -1.0f, 1.0f,
                0.0f, 100.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);
        } else {
            const float fovY = 60.0f;
            const float aspect = (height_ > 0) ? float(width_) / float(height_) : 16.0f/9.0f;
            bx::mtxProj(
                proj_, fovY, aspect,
                0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);
        }
        // 保持 useExternalProj_ = false（表示使用内部投影）
    }

private:
    CameraMode mode_ = CameraMode::Perspective;
    int   width_ = 1280, height_ = 720;

    // 标记：是否使用外部注入的矩阵（设为 true 则 apply 不会重建对应部分）
    bool  useExternalView_ = false;
    bool  useExternalProj_ = false;

    float view_[16]{};
    float proj_[16]{};
};
