#pragma once
#include <bgfx/bgfx.h>
#include <glm/vec4.hpp>
#include <cassert>

// 名称速记：Lighting 把“方向光/点光/相机/曝光”等帧级量统一创建/上传

struct Lighting {
    glm::vec4 lightDir_ambient { -0.5f,-1.0f,-0.3f, 0.15f }; // xyz=方向(指向物体), w=环境
    glm::vec4 pointPos_radius  {  0.0f, 1.0f, 2.0f, 6.0f  }; // 点光pos+半径
    glm::vec4 pointCol_intensity{ 1.0f, 0.9f, 0.7f, 2.0f };  // 点光颜色+强度
    glm::vec4 viewPos_exposure {  0.0f, 0.0f, 3.0f, 1.0f };  // 相机位置 + 曝光

    bgfx::UniformHandle u_lightDir   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_pointPosRad= BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_pointColInt= BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_viewPosExp = BGFX_INVALID_HANDLE;

    void init() {
        auto U=[&](const char* n){ auto u=bgfx::createUniform(n,bgfx::UniformType::Vec4); assert(bgfx::isValid(u)); return u; };
        u_lightDir    = U("u_lightDir");
        u_pointPosRad = U("u_pointPosRad");
        u_pointColInt = U("u_pointColInt");
        u_viewPosExp  = U("u_viewPosExp");
    }
    void shutdown() {
        if (bgfx::isValid(u_lightDir))    bgfx::destroy(u_lightDir);
        if (bgfx::isValid(u_pointPosRad)) bgfx::destroy(u_pointPosRad);
        if (bgfx::isValid(u_pointColInt)) bgfx::destroy(u_pointColInt);
        if (bgfx::isValid(u_viewPosExp))  bgfx::destroy(u_viewPosExp);
    }
    void uploadPerFrame() const {
        bgfx::setUniform(u_lightDir,    &lightDir_ambient[0]);
        bgfx::setUniform(u_pointPosRad, &pointPos_radius[0]);
        bgfx::setUniform(u_pointColInt, &pointCol_intensity[0]);
        bgfx::setUniform(u_viewPosExp,  &viewPos_exposure[0]);
    }
};
