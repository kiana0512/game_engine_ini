#include "core/FrameTimer.h"

namespace ke
{
    // 构造：记录当前时间为 start_ 与 last_，其余清零
    FrameTimer::FrameTimer() noexcept
    {
        start_     = clock::now();
        last_      = start_;
        lastDelta_ = 0.0;
        emaDelta_  = 0.0;
        frames_    = 0;
    }

    // 每帧调用：返回 dt（秒），并更新内部状态
    double FrameTimer::tick() noexcept
    {
        const auto now = clock::now();

        // 关键：seconds 是类里定义的别名，需要加 FrameTimer:: 作用域
        const FrameTimer::seconds dt = now - last_;

        // 修正：now 是一个 time_point 变量，不是函数；不能写 now()
        last_ = now;

        // 修正：count 是函数，需要调用 count()
        lastDelta_ = dt.count();

        ++frames_;

        // 首帧：直接把 emaDelta_ 设为当前 dt，避免 1/0
        if (emaDelta_ <= 0.0)
        {
            emaDelta_ = lastDelta_;
        }
        else
        {
            emaDelta_ = alpha_ * lastDelta_ + (1.0 - alpha_) * emaDelta_;
        }
        return lastDelta_;
    }

    void FrameTimer::reset() noexcept
    {
        start_     = clock::now();
        last_      = start_;
        lastDelta_ = 0.0;
        emaDelta_  = 0.0;
        frames_    = 0;
    }

    // 修正：签名需与头文件一致，加上 const
    double FrameTimer::elapsed() const noexcept
    {
        return std::chrono::duration<double>(clock::now() - start_).count();
        // 也可以用：return seconds(clock::now() - start_).count();
    }

    void FrameTimer::setSmoothing(double a) noexcept
    {
        if (a < 0.01) a = 0.01;
        if (a > 0.95) a = 0.95;
        alpha_ = a;
    }
}
