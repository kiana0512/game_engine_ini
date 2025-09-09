#pragma once
#include <vector>
#include <bgfx/bgfx.h>
#include <bx/math.h>

// 最小 Mesh 组件：VB/IB、可选贴图、状态、索引数、模型矩阵
struct MeshComp
{
    bgfx::VertexBufferHandle vbh { BGFX_INVALID_HANDLE };
    bgfx::IndexBufferHandle  ibh { BGFX_INVALID_HANDLE };
    bgfx::TextureHandle      baseColor { BGFX_INVALID_HANDLE };
    uint32_t                 indexCount = 0;
    uint64_t                 state =
        BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
        BGFX_STATE_MSAA      | BGFX_STATE_DEPTH_TEST_LESS;

    float model[16];
    MeshComp() { bx::mtxIdentity(model); }
};

struct Scene
{
    std::vector<MeshComp> meshes;

    void clear() { meshes.clear(); } // 仅清容器，不负责销毁 bgfx 句柄
    bool empty() const { return meshes.empty(); }
};
