#include "PbrMaterial.h"
#include "gfx/texture/TextureLoader.h" // 你现有的创建函数：createTexture2DFromFile(...) :contentReference[oaicite:5]{index=5}
#include <cassert>

static bgfx::UniformHandle U(const char* n, bgfx::UniformType::Enum t) {
    auto u = bgfx::createUniform(n, t);
    assert(bgfx::isValid(u) && "createUniform failed");
    return u;
}

bool PbrMaterialManager::init() { m_pool.reserve(128); return true; }

void PbrMaterialManager::shutdown() {
    for (auto& m : m_pool) {
        if (bgfx::isValid(m.program)) bgfx::destroy(m.program);
        if (bgfx::isValid(m.u_baseColorFactor)) bgfx::destroy(m.u_baseColorFactor);
        if (bgfx::isValid(m.u_mrFactor))        bgfx::destroy(m.u_mrFactor);
        if (bgfx::isValid(m.u_emissive))        bgfx::destroy(m.u_emissive);
        if (bgfx::isValid(m.u_flags))           bgfx::destroy(m.u_flags);
        if (bgfx::isValid(m.s_baseColor)) bgfx::destroy(m.s_baseColor);
        if (bgfx::isValid(m.s_mr))        bgfx::destroy(m.s_mr);
        if (bgfx::isValid(m.s_normal))    bgfx::destroy(m.s_normal);
        if (bgfx::isValid(m.s_ao))        bgfx::destroy(m.s_ao);
        if (bgfx::isValid(m.s_emissive))  bgfx::destroy(m.s_emissive);
        if (bgfx::isValid(m.t_baseColor)) bgfx::destroy(m.t_baseColor);
        if (bgfx::isValid(m.t_mr))        bgfx::destroy(m.t_mr);
        if (bgfx::isValid(m.t_normal))    bgfx::destroy(m.t_normal);
        if (bgfx::isValid(m.t_ao))        bgfx::destroy(m.t_ao);
        if (bgfx::isValid(m.t_emissive))  bgfx::destroy(m.t_emissive);
    }
    m_pool.clear();
}

PbrMatHandle PbrMaterialManager::create(const PbrMaterialDesc& d) {
    PbrMaterialGPU m{};
    // 常量/采样器（名字与 fs_pbr_mr.sc 对齐）
    m.u_baseColorFactor = U("u_baseColorFactor", bgfx::UniformType::Vec4);
    m.u_mrFactor        = U("u_mrFactor",        bgfx::UniformType::Vec4);
    m.u_emissive        = U("u_emissive",        bgfx::UniformType::Vec4);
    m.u_flags           = U("u_matFlags",        bgfx::UniformType::Vec4);
    m.s_baseColor = U("s_baseColor", bgfx::UniformType::Sampler);
    m.s_mr        = U("s_mr",        bgfx::UniformType::Sampler);
    m.s_normal    = U("s_normal",    bgfx::UniformType::Sampler);
    m.s_ao        = U("s_ao",        bgfx::UniformType::Sampler);
    m.s_emissive  = U("s_emissive",  bgfx::UniformType::Sampler);

    // 纹理（空路径 → 占位）
    m.t_baseColor = d.texBaseColor.empty()         ? solid1x1(255,255,255,true) : loadTexCached(d.texBaseColor, true);
    m.t_mr        = d.texMetallicRoughness.empty() ? solid1x1(255,255,255,false): loadTexCached(d.texMetallicRoughness, false);
    m.t_normal    = d.texNormal.empty()            ? solid1x1(128,128,255,false): loadTexCached(d.texNormal, false);
    m.t_ao        = d.texOcclusion.empty()         ? solid1x1(255,255,255,false): loadTexCached(d.texOcclusion, false);
    m.t_emissive  = d.texEmissive.empty()          ? solid1x1(0,0,0,true)       : loadTexCached(d.texEmissive, true);

    if (!d.texBaseColor.empty())        m.flags |= (1u<<0);
    if (!d.texMetallicRoughness.empty())m.flags |= (1u<<1);
    if (!d.texNormal.empty())           m.flags |= (1u<<2);
    if (!d.texOcclusion.empty())        m.flags |= (1u<<3);
    if (!d.texEmissive.empty())         m.flags |= (1u<<4);

    m.state = d.twoSided
      ? BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS
      : BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW;

    // 先不绑定 Program（由 ForwardPBR 填充）
    m.program = BGFX_INVALID_HANDLE;

    PbrMatHandle h = (PbrMatHandle)m_pool.size();
    m_pool.push_back(m);

    // 初值写一次（便于 debug）
    float mr[4] = { d.metallic, d.roughness, 0, 0 };
    bgfx::setUniform(m.u_baseColorFactor, &d.baseColorFactor[0]);
    bgfx::setUniform(m.u_mrFactor, mr);
    float em[4] = { d.emissive.x, d.emissive.y, d.emissive.z, 0 };
    bgfx::setUniform(m.u_emissive, em);
    return h;
}
void PbrMaterialManager::destroy(PbrMatHandle) { /* 简化：暂不回收槽位 */ }

bgfx::TextureHandle PbrMaterialManager::loadTexCached(const std::string& path, bool srgb) {
    auto it = m_texCache.find(path);
    if (it != m_texCache.end()) return it->second;
    auto t = createTexture2DFromFile(path, srgb, 0); // 你现有的加载接口 :contentReference[oaicite:6]{index=6}
    if (bgfx::isValid(t)) m_texCache[path] = t;
    return t;
}
bgfx::TextureHandle PbrMaterialManager::solid1x1(uint8_t r,uint8_t g,uint8_t b,bool srgb) {
    // 复用你的 TextureLoader；也可改成手工内存贴图
    std::vector<uint8_t> px = { r,g,b,255 };
    const bgfx::Memory* mem = bgfx::copy(px.data(), 4);
    return bgfx::createTexture2D(1,1,false,1,bgfx::TextureFormat::RGBA8,0,mem);
}
