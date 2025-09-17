#pragma once
#include <bgfx/bgfx.h>
#include <glm/mat4x4.hpp>
#include "gfx/material/PbrMaterial.h"
#include "gfx/lighting/Lighting.h"

// 名称速记：ForwardPBR 管线 = “上传光照 + 绑定材质 + 提交网格”的封装

class ForwardPBR {
public:
    bool init();
    void shutdown();

    Lighting& lighting() { return m_light; }

    // 给材质池绑定本管线着色器（或在创建材质后单独赋值）
    void attachProgramTo(PbrMaterialGPU& m);

    void draw(const glm::mat4& model,
              bgfx::VertexBufferHandle vbh,
              bgfx::IndexBufferHandle  ibh,
              const PbrMaterialGPU&    mat,
              uint8_t viewId = 0);

private:
    bgfx::ShaderHandle  m_vs = BGFX_INVALID_HANDLE;
    bgfx::ShaderHandle  m_fs = BGFX_INVALID_HANDLE;
    Lighting            m_light;
};
