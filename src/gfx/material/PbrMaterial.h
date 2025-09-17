#pragma once
#include <bgfx/bgfx.h>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <unordered_map>
#include <vector>

// 名称速记：Desc=CPU侧描述；GPU=GPU侧句柄集合；Manager=创建/缓存/销毁

struct PbrMaterialDesc {
    glm::vec4 baseColorFactor{1,1,1,1};
    float     metallic{1.0f};
    float     roughness{1.0f};
    glm::vec3 emissive{0,0,0};
    // 贴图路径（可为空）；sRGB: baseColor/emissive；Linear: mr/normal/ao
    std::string texBaseColor, texMetallicRoughness, texNormal, texOcclusion, texEmissive;
    bool twoSided{false};
};

struct PbrMaterialGPU {
    bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;

    // 常量
    bgfx::UniformHandle u_baseColorFactor = BGFX_INVALID_HANDLE; // vec4
    bgfx::UniformHandle u_mrFactor        = BGFX_INVALID_HANDLE; // x=metallic y=roughness
    bgfx::UniformHandle u_emissive        = BGFX_INVALID_HANDLE; // vec3
    bgfx::UniformHandle u_flags           = BGFX_INVALID_HANDLE; // bit 标志

    // 采样器
    bgfx::UniformHandle s_baseColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_mr        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_normal    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_ao        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_emissive  = BGFX_INVALID_HANDLE;

    // 纹理对象
    bgfx::TextureHandle t_baseColor = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle t_mr        = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle t_normal    = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle t_ao        = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle t_emissive  = BGFX_INVALID_HANDLE;

    uint64_t state = 0;
    uint32_t flags = 0; // bit0:baseColor bit1:mr bit2:normal bit3:ao bit4:emissive
    bool valid() const { return bgfx::isValid(program); }
};

using PbrMatHandle = uint16_t;

class PbrMaterialManager {
public:
    bool init();
    void shutdown();

    PbrMatHandle create(const PbrMaterialDesc& d);
    const PbrMaterialGPU& get(PbrMatHandle h) const { return m_pool[h]; }
    void destroy(PbrMatHandle h);

private:
    std::vector<PbrMaterialGPU> m_pool;
    std::unordered_map<std::string, bgfx::TextureHandle> m_texCache;

    bgfx::TextureHandle loadTexCached(const std::string& path, bool srgb);
    bgfx::TextureHandle solid1x1(uint8_t r, uint8_t g, uint8_t b, bool srgb);
};
