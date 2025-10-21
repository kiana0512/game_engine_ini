#include "ClearPass.h"           // 自己的声明
#include <bgfx/bgfx.h>           // bgfx API

void ClearPass::execute(const RenderContext& rc) {
    // 设置清屏标志位：颜色+深度+模板
    const uint16_t clearFlags = BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL;

    // 将清屏参数写到该 view
    bgfx::setViewClear(rc.view, clearFlags,
                       // 颜色需要转换成 0xAARRGGBB 在 setViewClear 的另一个重载也可以传浮点，这里直接传浮点数组更直观
                       // 注意：这里使用的是支持浮点的那个重载（新版 bgfx 提供）
                       // 如果你本地的 bgfx 版本只接受整数颜色，请改用 bgfx::setViewClear(view, flags, rgba, depth, stencil) 的整型形式。
                       // 为了兼容，我们直接用浮点重载：
                       m_clearRGBA[0], m_clearRGBA[1], m_clearRGBA[2], m_clearRGBA[3],
                       m_clearDepth, m_clearStencil);

    // 触发一次空提交（不画东西，但告诉 bgfx 这帧该 view 是“活”的）
    bgfx::touch(rc.view);
}
