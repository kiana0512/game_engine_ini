#pragma once
#include <vector>
#include <string>
#include <bgfx/bgfx.h>
#include <bx/math.h>

/**
 * Mesh 组件（最小可用 + Day4 扩展）：
 * - GPU 资源（VB/IB/可选贴图）
 * - 渲染状态与索引数
 * - 变换参数（SRT：position/rotation/scale）
 * - 最终模型矩阵（提交前由 updateModel() 生成）
 * - Day4: 导出用 CPU 缓存
 */
struct MeshComp
{
    // —— GPU 资源句柄 —— //
    bgfx::VertexBufferHandle vbh{BGFX_INVALID_HANDLE};
    bgfx::IndexBufferHandle  ibh{BGFX_INVALID_HANDLE};
    bgfx::TextureHandle      baseColor{BGFX_INVALID_HANDLE};

    // —— 渲染状态与索引数 —— //
    uint64_t state =
        BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
        BGFX_STATE_MSAA | BGFX_STATE_DEPTH_TEST_LESS;
    uint32_t indexCount = 0;

    // —— 变换参数（世界空间）—— //
    // 约定 rotation 为 XYZ 欧拉角（单位：弧度），scale 支持非均匀缩放
    float position[3] = {0.0f, 0.0f, 0.0f};
    float rotation[3] = {0.0f, 0.0f, 0.0f};
    float scale[3]    = {1.0f, 1.0f, 1.0f};

    // —— 模型矩阵（由 updateModel() 自动生成）—— //
    float model[16];

    MeshComp() { bx::mtxIdentity(model); }

    // 将 SRT 组合为模型矩阵
    void updateModel()
    {
        bx::mtxSRT(
            model,
            scale[0], scale[1], scale[2],
            rotation[0], rotation[1], rotation[2],
            position[0], position[1], position[2]);
    }

    // —— Day4: 导出用 CPU 缓存（与 GltfLoader 顶点字段一致）—— //
    struct MeshVertexExport
    {
        float px, py, pz;
        float nx, ny, nz;
        float u, v;
    };

    std::vector<MeshVertexExport> cpuVertices;
    std::vector<uint32_t>         cpuIndices;

    // 贴图源路径（若有）
    std::string baseColorPath;
};

/**
 * 场景容器：
 * - 管理一组 MeshComp
 * - 每帧调用 update() 生成/刷新每个网格的模型矩阵
 */
struct Scene
{
    std::vector<MeshComp> meshes;

    void clear() { meshes.clear(); } // 注意：仅清空容器，不负责销毁 bgfx 句柄
    bool empty() const { return meshes.empty(); }

    // 每帧更新：把 SRT 写入 model 矩阵
    void update()
    {
        for (auto &m : meshes)
        {
            m.updateModel();
        }
    }
};
