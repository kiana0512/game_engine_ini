#pragma once

#include "RenderPass.h"               // RenderPass / RenderContext / DrawItem
#include <bgfx/bgfx.h>
#include <utility>                    // std::exchange

// 前置：我们会在 .cpp 里包含你的相机头文件以获得 view/proj 矩阵
// 之所以不在头里包含，是为了降低编译依赖。

// ForwardPBRPass：最小前向 PBR 提交通道
class ForwardPBRPass final : public RenderPass {
public:
    ForwardPBRPass() = default;

    ~ForwardPBRPass() override {
        // 析构：释放本 Pass 拥有的 GPU 资源（判无效句柄以防重复销毁）
        if (bgfx::isValid(m_program))            bgfx::destroy(m_program);
        if (bgfx::isValid(u_BaseColor))          bgfx::destroy(u_BaseColor);
        if (bgfx::isValid(u_MetallicRoughness))  bgfx::destroy(u_MetallicRoughness);
        if (bgfx::isValid(u_CamPos_Time))        bgfx::destroy(u_CamPos_Time);
        if (bgfx::isValid(s_BaseColor))          bgfx::destroy(s_BaseColor);
        if (bgfx::isValid(s_Normal))             bgfx::destroy(s_Normal);
        if (bgfx::isValid(s_MR))                 bgfx::destroy(s_MR);
    }

    // 懒加载：第一次调用 prepare() 时创建 shader program / uniforms
    void prepare(const RenderContext& rc) override;

    // 真正提交 draw calls
    void execute(const RenderContext& rc) override;

private:
    // 着色程序（VS/FS）：用你工程里已有的装载工具创建（详见 .cpp）
    bgfx::ProgramHandle m_program{BGFX_INVALID_HANDLE};

    // Per-Material：基础色 + (金属, 粗糙)；Per-Frame：摄像机位置 + 时间（方便做动画/IBL 混合）
    bgfx::UniformHandle u_BaseColor{BGFX_INVALID_HANDLE};         // vec3 + padding
    bgfx::UniformHandle u_MetallicRoughness{BGFX_INVALID_HANDLE}; // vec2
    bgfx::UniformHandle u_CamPos_Time{BGFX_INVALID_HANDLE};       // vec4(cam.xyz, time)

    // 采样器句柄（sampler uniform）：和 shader 里的 `SAMPLER2D(s_BaseColor)` 名字要一致
    bgfx::UniformHandle s_BaseColor{BGFX_INVALID_HANDLE};
    bgfx::UniformHandle s_Normal{BGFX_INVALID_HANDLE};
    bgfx::UniformHandle s_MR{BGFX_INVALID_HANDLE};
};
