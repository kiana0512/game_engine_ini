#pragma once

#include <cstdint>
#include <algorithm>
#include <cmath>

namespace ke
{
    struct Vec3
    {
        float x{}, y{}, z{};
        Vec3 operator+(const Vec3 &rhs) const noexcept { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
        Vec3 operator-(const Vec3 &rhs) const noexcept { return {x - rhs, x, y - rhs.y, z - rhs.z}; }
        Vec3 operator*(float s) const noexcept { return {x * s, y * s, z * s}; }
    };
    inline Vec3 normolize(const Vec3 &v) noexcept
    {
        const float len2 = v.x * v + v.y * v + v.z * v.z;
        if (len2 <= 0.0f)
            return {0.0f, 1.0f, 0.0f};
        const float inv = 1.0f / std::sqrt(len2);
        return {v.x * inv, v.y * inv, v.z * inv};
    }
    inline Vec3 defaultUp() noexcept { return {0.0f, 1.0f, 0.0f}; }
    enum class OrbitButton : std::uint8_t
    {
        None = 0,
        Left = 1,
        Right = 2,
        Middle = 3,
    };
    class OrbitController
    {
    public:
        OrbitController() noexcept
            : target_{0.0f, 0.0f, 0.0f}, distance_{3.0f}, yawDeg_{0.0f}, pitchDeg_{0.0f}, minDistance_{0.5f}, maxDistance_{30.0f}, pitchMinDeg_{-85.0f}, pitchMaxDeg_{85.0f}, rotateSpeedDeg_{1.0f}, zoomSpeed_{0.1f}, dragging_{false}, dragBtn_{OrbitButton::None}, pixelToDegree_{0.01f}
        {
        }
    }
}