#pragma once
/*
  OrbitController.h
  功能：根据鼠标右键拖动(旋转) + 滚轮(缩放) 计算相机的 eye/at/up。
  用法：每帧喂它输入 → 调用 update(dt) → 读出 eye()/at()/up()。
*/

#include <cstdint>     // uint8_t 等固定宽度整数
#include <algorithm>   // std::clamp
#include <cmath>       // std::sin/std::cos/std::sqrt

namespace ke { // 命名空间：给你的代码一个“姓氏”，避免重名冲突

// =============== 语法：struct 是什么？ ==================
// struct 和 class 很像：都是“装数据 + 函数”的盒子。
// 区别：struct 默认 public；class 默认 private。
// 这里用 struct 存纯数据更顺手（默认就能公开访问）。
struct Vec3 {
    float x{}, y{}, z{};  // “统一初始化”{}：把基础类型初始化为 0（防野值）

    // 运算符重载：让向量加减像数学一样写
    // “const 成员函数”：承诺不改对象内部数据。好处：在 const 环境能用。
    // “noexcept”：承诺不抛异常，底层数学更高效安全。
    Vec3 operator+(const Vec3& rhs) const noexcept { return {x+rhs.x, y+rhs.y, z+rhs.z}; }
    Vec3 operator-(const Vec3& rhs) const noexcept { return {x-rhs.x, y-rhs.y, z-rhs.z}; }
    Vec3 operator*(float s)          const noexcept { return {x*s,   y*s,   z*s  }; }
};

// “自由函数”：不属于类，放命名空间里。
// inline：提示编译器“这个函数很短，可以内联到调用点”，减少函数调用开销。
inline Vec3 normalize(const Vec3& v) noexcept {
    const float len2 = v.x*v.x + v.y*v.y + v.z*v.z;
    if (len2 <= 0.0f) return {0.0f, 1.0f, 0.0f};   // 退化：返回一个“向上”方向
    const float inv = 1.0f / std::sqrt(len2);
    return { v.x*inv, v.y*inv, v.z*inv };
}

inline Vec3 defaultUp() noexcept { return {0.0f, 1.0f, 0.0f}; }

// =============== 语法：enum class 是什么？ ==================
// “强作用域枚举”：枚举名不会泄漏到全局，必须写 OrbitButton::Right 这种全名；
// 也不会被当成 int 隐式转换（更安全）。
enum class OrbitButton : std::uint8_t {
    None = 0, Left = 1, Right = 2, Middle = 3,
};

class OrbitController {
public:
    // =============== 语法：构造函数与“成员初始化列表” ==================
    // 构造函数：对象“出生时”自动执行；用“冒号”给每个成员一个初值（比在花括号里赋值更高效）。
    // 注意：实际初始化顺序按“成员声明的顺序”，不是你写的冒号顺序。
    OrbitController() noexcept
    : target_{0.0f, 0.0f, 0.0f}   // 目标点
    , distance_{3.0f}             // 距离
    , yawDeg_{0.0f}               // 左右转角（度）
    , pitchDeg_{0.0f}             // 上下转角（度）
    , minDistance_{0.5f}          // 限制：最近
    , maxDistance_{30.0f}         // 限制：最远
    , pitchMinDeg_{-85.0f}        // 限制：最低俯仰（度）
    , pitchMaxDeg_{ 85.0f}        // 限制：最高俯仰（度）
    , rotateSpeedDeg_{1.0f}       // “像素→度”的缩放（右键拖动灵敏度）
    , zoomSpeed_{0.1f}            // 缩放灵敏度
    , dragging_{false}            // 是否正在拖拽
    , dragBtn_{OrbitButton::None} // 哪个按钮在拖
    , pixelToDegree_{0.01f}       // 额外系数（像素→角度）
    {}

    // =============== set*：普通成员函数、值传递与 const 引用 ==================
    // 传 const 引用（const Vec3&）：避免拷贝，又不允许在函数里修改它。
    void setTarget(const Vec3& p) noexcept { target_ = p; }

    void setAnglesDeg(float yawDeg, float pitchDeg) noexcept {
        yawDeg_   = yawDeg;
        // std::clamp(x, a, b)：把 x 限到 [a, b] 之间
        pitchDeg_ = std::clamp(pitchDeg, pitchMinDeg_, pitchMaxDeg_);
    }

    void setDistance(float d) noexcept {
        distance_ = std::clamp(d, minDistance_, maxDistance_);
    }

    void setDistanceRange(float minD, float maxD) noexcept;
    void setPitchRangeDeg(float minDeg, float maxDeg) noexcept;

    // =============== 输入事件（Begin/Move/End） ==================
    void beginDrag(OrbitButton btn) noexcept { dragging_ = true; dragBtn_ = btn; }
    void dragDelta(float dx, float dy) noexcept; // 声明，具体实现放 .cpp
    void endDrag() noexcept { dragging_ = false; dragBtn_ = OrbitButton::None; }

    void onScroll(float wheelDelta) noexcept {
        // 滚轮：正数拉近，负数拉远
        const float factor = 1.0f - wheelDelta * zoomSpeed_;
        distance_ = std::clamp(distance_ * factor, minDistance_, maxDistance_);
    }

    // 每帧更新（有需要时可加平滑/阻尼，目前空实现）
    void update(double /*dtSec*/) noexcept {}

    // =============== Getter：const + noexcept + inline 小函数 ==================
    // const：承诺不改成员；noexcept：承诺不抛异常；小函数直接写类里，便于内联。
    Vec3 eye() const noexcept {
        const float yaw   = deg2rad(yawDeg_);
        const float pitch = deg2rad(pitchDeg_);
        const float cy = std::cos(yaw),   sy = std::sin(yaw);
        const float cp = std::cos(pitch), sp = std::sin(pitch);
        // 右手系：用球坐标求出“相机相对目标”的向量
        Vec3 forward{ cp*cy, sp, cp*sy };
        return target_ + forward * distance_;
    }

    Vec3 at() const noexcept { return target_; }
    Vec3 up() const noexcept { return defaultUp(); }

    float yawDeg()   const noexcept { return yawDeg_; }
    float pitchDeg() const noexcept { return pitchDeg_; }
    float distance() const noexcept { return distance_; }

    void  setPixelToDegree(float k) noexcept { pixelToDegree_ = k; }
    float pixelToDegree() const noexcept { return pixelToDegree_; }

private:
    // =============== 小工具：度↔弧度（静态内联） ==================
    // static：属于类，不属于某个对象；inline：建议编译器内联。
    static inline float deg2rad(float deg) noexcept { return deg * 0.01745329251994329577f; }

private:
    // =============== 成员数据：统一初始化，避免野值 ==================
    Vec3  target_{};
    float distance_{};

    float yawDeg_{};
    float pitchDeg_{};

    float minDistance_{};
    float maxDistance_{};
    float pitchMinDeg_{};
    float pitchMaxDeg_{};

    float rotateSpeedDeg_{};
    float zoomSpeed_{};
    bool  dragging_{};
    OrbitButton dragBtn_{};

    float pixelToDegree_{}; // 鼠标像素→角度的比例（额外系数）
};

} // namespace ke
