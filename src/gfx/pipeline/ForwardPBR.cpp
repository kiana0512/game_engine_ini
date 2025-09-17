#include "ForwardPBR.h"
#include "gfx/shaders/shader_utils.h" // ke_loadShaderFile/ke_loadProgramDx11 :contentReference[oaicite:7]{index=7}
#include <glm/gtc/type_ptr.hpp>

extern bgfx::ShaderHandle ke_loadShaderFile(const std::string&); // 声明以便使用

bool ForwardPBR::init() {
    m_light.init();
    m_vs = ke_loadShaderFile("vs_pbr.bin");
    m_fs = ke_loadShaderFile("fs_pbr_mr.bin");
    return bgfx::isValid(m_vs) && bgfx::isValid(m_fs);
}
void ForwardPBR::shutdown() {
    m_light.shutdown();
    if (bgfx::isValid(m_vs)) bgfx::destroy(m_vs);
    if (bgfx::isValid(m_fs)) bgfx::destroy(m_fs);
}
void ForwardPBR::attachProgramTo(PbrMaterialGPU& m) {
    if (!bgfx::isValid(m.program)) {
        auto prog = bgfx::createProgram(m_vs, m_fs, /*destroyShaders*/false);
        if (bgfx::isValid(prog)) m.program = prog;
    }
}
void ForwardPBR::draw(const glm::mat4& model,
                      bgfx::VertexBufferHandle vbh,
                      bgfx::IndexBufferHandle  ibh,
                      const PbrMaterialGPU&    mat,
                      uint8_t viewId) {
    m_light.uploadPerFrame();

    bgfx::setTransform(glm::value_ptr(model));
    bgfx::setVertexBuffer(0, vbh);
    bgfx::setIndexBuffer(ibh);

    float flags[4] = { (float)mat.flags, 0,0,0 };
    bgfx::setUniform(mat.u_flags, flags);
    bgfx::setTexture(0, mat.s_baseColor, mat.t_baseColor);
    bgfx::setTexture(1, mat.s_mr,        mat.t_mr);
    bgfx::setTexture(2, mat.s_normal,    mat.t_normal);
    bgfx::setTexture(3, mat.s_ao,        mat.t_ao);
    bgfx::setTexture(4, mat.s_emissive,  mat.t_emissive);

    bgfx::setState(mat.state);

    bgfx::ProgramHandle p = mat.program;
    if (!bgfx::isValid(p)) p = bgfx::createProgram(m_vs, m_fs, false);
    bgfx::submit(viewId, p);
}
