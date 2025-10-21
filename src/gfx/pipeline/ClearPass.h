#pragma once

#include "RenderPass.h"   // 依赖 RenderPass/RenderContext
#include <array>          // std::array 固定大小数组（栈上、无额外分配）

// ClearPass：只负责设置 view 的清屏颜色/深度/模板并“touch”一次 view。
class ClearPass final : public RenderPass {
public:
    // explicit：防止单参数构造发生隐式转换
    explicit ClearPass(std::array<float, 4> clearRGBA = {0.1f, 0.1f, 0.1f, 1.0f},
                       float clearDepth = 1.0f,
                       uint8_t clearStencil = 0) noexcept
        : m_clearRGBA(clearRGBA),
          m_clearDepth(clearDepth),
          m_clearStencil(clearStencil)
    {}

    // 准备阶段对本 Pass 来说什么都不做
    void prepare(const RenderContext& /*rc*/) override {}

    // 关键：设置清屏并触发一个空提交（bgfx::touch）确保该 view 被处理
    void execute(const RenderContext& rc) override;

private:
    std::array<float, 4> m_clearRGBA;   // 清屏颜色
    float   m_clearDepth;               // 清屏深度（1.0 表示最远）
    uint8_t m_clearStencil;             // 清屏模板
};
