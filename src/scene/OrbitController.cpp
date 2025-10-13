#include "scene/OrbitController.h"

// 说明：这里实现“较长”的函数，让头文件更干净；
// 短小的 getter/工具函数留在头文件利于内联。

namespace ke {

void OrbitController::setDistanceRange(float minD, float maxD) noexcept {
    if (minD > maxD) std::swap(minD, maxD);              // 保证 min <= max
    minDistance_ = std::max(0.01f, minD);                // 距离不能小于 0
    maxDistance_ = std::max(minDistance_, maxD);         // 保证 max >= min
    distance_    = std::clamp(distance_, minDistance_, maxDistance_);
}

void OrbitController::setPitchRangeDeg(float minDeg, float maxDeg) noexcept {
    if (minDeg > maxDeg) std::swap(minDeg, maxDeg);
    // 俯仰角限制（避免相机翻过去）：[-89.9, +89.9]
    pitchMinDeg_ = std::max(-89.9f, minDeg);
    pitchMaxDeg_ = std::min( 89.9f, maxDeg);
    pitchDeg_    = std::clamp(pitchDeg_, pitchMinDeg_, pitchMaxDeg_);
}

void OrbitController::dragDelta(float dx, float dy) noexcept {
    if (!dragging_) return;
    // 像素 → 角度：两个“灵敏度”相乘
    yawDeg_   += dx * rotateSpeedDeg_ * pixelToDegree_;
    pitchDeg_ -= dy * rotateSpeedDeg_ * pixelToDegree_; // 鼠标往上拖，视角向下（减号）
    pitchDeg_  = std::clamp(pitchDeg_, pitchMinDeg_, pitchMaxDeg_);
}

} // namespace ke
