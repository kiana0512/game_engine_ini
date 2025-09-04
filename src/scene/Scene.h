#pragma once
#include <bgfx/bgfx.h>
#include <vector>
#include <string>

struct Transform {
    float m[16]; // 世界矩阵（右手，行主序/列主序不重要，最终传 bgfx::setTransform 用即可）
};

struct MeshComp {
    bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle  ibh = BGFX_INVALID_HANDLE;
    uint32_t indexCount = 0;
    bgfx::TextureHandle baseColor = BGFX_INVALID_HANDLE;
    uint64_t state = 0; // BGFX_STATE_*
};

struct CameraComp {
    float view[16];
    float proj[16];
};

struct LightDir {
    float dir[3] = { -0.5f, -1.0f, -0.3f }; // 世界空间方向（已归一化）
};

struct Scene {
    std::vector<Transform> transforms;
    std::vector<MeshComp>  meshes;
    CameraComp camera{};
    LightDir   sun{};

    size_t addMesh(const MeshComp& m, const Transform& t) {
        meshes.push_back(m);
        transforms.push_back(t);
        return meshes.size()-1;
    }
};
