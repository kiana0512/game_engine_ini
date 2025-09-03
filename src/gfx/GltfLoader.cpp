#include "GltfLoader.h"
#include <tiny_gltf.h>
#include <spdlog/spdlog.h>
#include <cassert>
#include <filesystem>

namespace fs = std::filesystem;

// 只在本 TU 实现 tinygltf
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION       // tinygltf 会用到（与我们自己的 stb_image 实现分离 TU 没问题）
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

static std::string dirOf(const std::string& path) {
    try { return fs::path(path).parent_path().string(); }
    catch (...) { return "."; }
}

template<typename T>
static const T* getAttribPtr(const tinygltf::Model& model,
                             const tinygltf::Accessor& acc)
{
    const tinygltf::BufferView& bv = model.bufferViews[acc.bufferView];
    const tinygltf::Buffer&     b  = model.buffers[bv.buffer];
    return reinterpret_cast<const T*>(&b.data[bv.byteOffset + acc.byteOffset]);
}

bool loadGltfMesh(const std::string& path, MeshData& out,
                  std::string* warn, std::string* err)
{
    out = {};

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string tw, te;

    bool ok = false;
    if (fs::path(path).extension() == ".glb")
        ok = loader.LoadBinaryFromFile(&model, &tw, &te, path);
    else
        ok = loader.LoadASCIIFromFile(&model, &tw, &te, path);

    if (warn) *warn = tw;
    if (err)  *err  = te;

    if (!ok) {
        spdlog::error("[glTF] load failed: {} (warn='{}' err='{}')", path, tw, te);
        return false;
    }
    if (model.meshes.empty()) {
        spdlog::error("[glTF] no meshes in {}", path);
        return false;
    }

    const tinygltf::Mesh& mesh = model.meshes[0];
    if (mesh.primitives.empty()) {
        spdlog::error("[glTF] no primitives in first mesh: {}", path);
        return false;
    }

    const tinygltf::Primitive& prim = mesh.primitives[0];
    if (prim.mode != TINYGLTF_MODE_TRIANGLES) {
        spdlog::warn("[glTF] primitive mode != TRIANGLES; got {}", prim.mode);
    }

    // POSITION
    auto itPos = prim.attributes.find("POSITION");
    if (itPos == prim.attributes.end()) {
        spdlog::error("[glTF] missing POSITION");
        return false;
    }
    const tinygltf::Accessor& accPos = model.accessors[itPos->second];
    const float* pos = getAttribPtr<float>(model, accPos);
    const size_t vcount = accPos.count;

    // NORMAL（可选）
    const float* nrm = nullptr;
    auto itNrm = prim.attributes.find("NORMAL");
    if (itNrm != prim.attributes.end()) {
        const tinygltf::Accessor& accNrm = model.accessors[itNrm->second];
        nrm = getAttribPtr<float>(model, accNrm);
    }

    // TEXCOORD_0（可选）
    const float* uv = nullptr;
    auto itUv = prim.attributes.find("TEXCOORD_0");
    if (itUv != prim.attributes.end()) {
        const tinygltf::Accessor& accUv = model.accessors[itUv->second];
        uv = getAttribPtr<float>(model, accUv);
    }

    // 组装顶点
    out.vertices.resize(vcount);
    for (size_t i = 0; i < vcount; ++i) {
        MeshVertex v{};
        v.px = pos[i*3+0]; v.py = pos[i*3+1]; v.pz = pos[i*3+2];
        if (nrm) { v.nx = nrm[i*3+0]; v.ny = nrm[i*3+1]; v.nz = nrm[i*3+2]; }
        else     { v.nx = 0; v.ny = 0; v.nz = 1; }
        if (uv)  { v.u  = uv[i*2+0]; v.v  = uv[i*2+1]; }
        else     { v.u  = 0; v.v  = 0; }
        out.vertices[i] = v;
    }

    // 索引
    if (prim.indices >= 0) {
        const tinygltf::Accessor& accIdx = model.accessors[prim.indices];
        const tinygltf::BufferView& bv = model.bufferViews[accIdx.bufferView];
        const tinygltf::Buffer& b = model.buffers[bv.buffer];
        const uint8_t* base = &b.data[bv.byteOffset + accIdx.byteOffset];

        out.indices.resize(accIdx.count);
        if (accIdx.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            const uint16_t* src = reinterpret_cast<const uint16_t*>(base);
            for (size_t i = 0; i < accIdx.count; ++i) out.indices[i] = src[i];
        } else if (accIdx.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
            const uint32_t* src = reinterpret_cast<const uint32_t*>(base);
            for (size_t i = 0; i < accIdx.count; ++i) out.indices[i] = (uint16_t)src[i]; // 直接窄化（大网格以后再做 32bit）
        } else if (accIdx.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            const uint8_t* src = reinterpret_cast<const uint8_t*>(base);
            for (size_t i = 0; i < accIdx.count; ++i) out.indices[i] = src[i];
        } else {
            spdlog::error("[glTF] index component type unsupported: {}", accIdx.componentType);
            return false;
        }
    } else {
        // 无索引：自动生成 [0..vcount)
        out.indices.resize(vcount);
        for (size_t i = 0; i < vcount; ++i) out.indices[i] = (uint16_t)i;
    }

    // 取 BaseColor 贴图（若使用材质）
    if (prim.material >= 0) {
        const auto& mtl = model.materials[prim.material];
        const auto& pbr = mtl.pbrMetallicRoughness;
        if (pbr.baseColorTexture.index >= 0) {
            const int texIdx = pbr.baseColorTexture.index;
            const auto& tex  = model.textures[texIdx];
            if (tex.source >= 0) {
                const auto& img = model.images[tex.source];
                if (!img.uri.empty()) {
                    out.baseColorTexPath = (fs::path(dirOf(path)) / img.uri).string();
                } else {
                    // 嵌入式贴图（.glb，image.image 有字节）——简化起见：先不处理，后续版本我们支持内存创建
                    spdlog::warn("[glTF] embedded baseColor texture (GLB) not exported as external file; use glTF Separate for now.");
                }
            }
        }
    }

    spdlog::info("[glTF] loaded: {}  verts={}  indices={}  tex='{}'",
        path, out.vertices.size(), out.indices.size(), out.baseColorTexPath);
    return true;
}
