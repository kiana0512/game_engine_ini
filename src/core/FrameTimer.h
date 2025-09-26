#pragma once // 只包含一次

#include <chrono>
#include <cstdint> // 固定宽度整数类型（std::uint64_t 等）

namespace ke
{
    class FrameTimer
    {
    public:
        // 类型别名（给冗长类型起短名）
        using clock      = std::chrono::high_resolution_clock;
        using time_point = clock::time_point;
        using seconds    = std::chrono::duration<double>;

        // 构造 / 析构 / 拷贝移动控制
        FrameTimer() noexcept;
        ~FrameTimer() = default;

        FrameTimer(const FrameTimer&)            = delete;
        FrameTimer& operator=(const FrameTimer&) = delete;
        FrameTimer(FrameTimer&&) noexcept        = default;
        FrameTimer& operator=(FrameTimer&&) noexcept = default;

        // 每帧调用：返回本帧 dt（秒）
        double tick() noexcept;

        // 重置到初始状态
        void reset() noexcept;

        // 只读查询（小函数适合内联）
        double        delta()   const noexcept { return lastDelta_; }
        double        fps()     const noexcept { return (emaDelta_ > 0.0) ? (1.0 / emaDelta_) : 0.0; }
        double        elapsed() const noexcept;
        std::uint64_t frameCount() const noexcept { return frames_; }

        // 设置 EMA 平滑因子（0~1）
        void setSmoothing(double alpha) noexcept;

    private:
        time_point    start_{};     // 起始时间点
        time_point    last_{};      // 上一帧时间点
        double        lastDelta_{}; // 上一帧 dt（秒）
        double        emaDelta_{};  // 平滑后的 dt（EMA）
        double        alpha_{0.10}; // EMA 平滑系数，默认 0.10
        std::uint64_t frames_{0};   // 帧计数
    };
} // namespace ke
