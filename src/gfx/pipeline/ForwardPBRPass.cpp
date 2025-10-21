// 本文件实现 ForwardPBRPass：从 RenderContext.drawList 遍历所有 DrawItem，
// 为每个物体绑定 VB/IB、设置模型矩阵/材质 uniform/贴图，然后 submit 到当前 view。

#include "ForwardPBRPass.h"     // 对应的声明（包含程序/Uniform 句柄成员）
#include "RenderPass.h"         // RenderContext / DrawItem / DrawKey 的定义
#include <bgfx/bgfx.h>          // bgfx 渲染 API
#include <bx/math.h>            // bx 数学库（这里主要是类型与工具）
#include "gfx/shaders/shader_utils.h" // 你的 shader 装载工具（ke_loadProgramDx11 等）

// 注意：我们不再 include "gfx/camera/Camera.h"
// Pass 只消费 RenderContext 里传进来的“纯数据”（view/proj 指针 + camPos）。

void ForwardPBRPass::prepare(const RenderContext& /*rc*/)
{
    // 懒加载：只在第一次需要时创建 shader 程序与 uniforms
    // 1) 程序（VS/FS）
    
    if (!bgfx::isValid(m_program)) {
        // 如果你项目里装载函数名不同（例如 ke_loadProgram 或 loadProgram），
        // 把下面这一行改成你工程的对应函数即可。
        m_program = ke_loadProgramDx11("vs_pbr", "fs_pbr"); // 加载/链接 VS+FS
    }

    // 2) Uniform（注意名称需要与着色器里保持一致）
    if (!bgfx::isValid(u_BaseColor))         u_BaseColor         = bgfx::createUniform("u_BaseColor",         bgfx::UniformType::Vec4);
    if (!bgfx::isValid(u_MetallicRoughness)) u_MetallicRoughness = bgfx::createUniform("u_MetallicRoughness", bgfx::UniformType::Vec4);
    if (!bgfx::isValid(u_CamPos_Time))       u_CamPos_Time       = bgfx::createUniform("u_CamPos_Time",       bgfx::UniformType::Vec4);

    // 3) 采样器（sampler uniform）
    if (!bgfx::isValid(s_BaseColor))         s_BaseColor = bgfx::createUniform("s_BaseColor", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(s_Normal))            s_Normal    = bgfx::createUniform("s_Normal",    bgfx::UniformType::Sampler);
    if (!bgfx::isValid(s_MR))                s_MR        = bgfx::createUniform("s_MR",        bgfx::UniformType::Sampler);
}

void ForwardPBRPass::execute(const RenderContext& rc)
{
    // 1) 设置本 view 的视图/投影矩阵（直接使用 RenderContext 里传入的指针）
    //    这一步把摄像机的姿态/投影作用到当前 bgfx 视图。
    bgfx::setViewTransform(rc.view, rc.viewMatrix, rc.projMatrix);

    // 2) 设置 Per-Frame uniform（摄像机位置 + 时间）
    //    约定：u_CamPos_Time = vec4(cam.xyz, time)
    float camPosTime[4] = { rc.camPos[0], rc.camPos[1], rc.camPos[2], rc.timeSec };
    bgfx::setUniform(u_CamPos_Time, camPosTime);

    // 3) 遍历 DrawList，逐个提交绘制
    for (const DrawItem& item : rc.drawList)   // const&：避免复制；范围 for 简洁
    {
        // 3.1 绑定顶点/索引缓冲区（slot 0）
        bgfx::setVertexBuffer(0, item.vbh);
        if (bgfx::isValid(item.ibh)) {
            bgfx::setIndexBuffer(item.ibh);
        }

        // 3.2 设置物体的模型矩阵（4×4，列主序）
        bgfx::setTransform(item.model);

        // 3.3 每材质 uniform：基础色（vec3 -> vec4，W 分量填 1）
        float baseColor4[4] = { item.baseColor[0], item.baseColor[1], item.baseColor[2], 1.0f };
        bgfx::setUniform(u_BaseColor, baseColor4);

        // 3.4 每材质 uniform：金属/粗糙（其余两分量留作扩展，例如 AO/flags）
        float mr4[4] = { item.metallic, item.roughness, 0.0f, 0.0f };
        bgfx::setUniform(u_MetallicRoughness, mr4);

        // 3.5 绑定贴图到固定采样槽（与 FS 着色器里的 sampler 声明保持一致）
        if (bgfx::isValid(item.baseColorTex)) bgfx::setTexture(0, s_BaseColor, item.baseColorTex); // BaseColor -> slot 0
        if (bgfx::isValid(item.normalTex))    bgfx::setTexture(1, s_Normal,    item.normalTex);    // Normal    -> slot 1
        if (bgfx::isValid(item.mrTex))        bgfx::setTexture(2, s_MR,        item.mrTex);        // M/R       -> slot 2

        // 3.6 设置固定渲染状态（深度测试、写入、剔除等）
        bgfx::setState(item.state);

        // 3.7 提交 draw call 到当前 view，使用我们的 PBR 程序
        bgfx::submit(rc.view, m_program);
    }
}
