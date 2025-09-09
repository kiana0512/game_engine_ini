#include "io/Exporter.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cmath>            // std::sqrtf
#include <spdlog/spdlog.h>
#include <bgfx/bgfx.h>
#include <bx/math.h>

#include "scene/Scene.h"    // 使用 MeshComp 及其嵌套类型

namespace fs = std::filesystem;

/**
 * OBJ 导出实现思路（在你的基础上修正）：
 * - 真正导出使用 Scene::MeshComp 中的 CPU 缓存：
 *     std::vector<MeshComp::MeshVertexExport> cpuVertices;
 *     std::vector<uint32_t>                   cpuIndices;
 *   这些由 Renderer::addMeshFromGltfToScene(...) 填好。
 * - 如果没有 CPU 数据，就导出一个占位三角形，确保流程可跑通。
 * - 顶点位置应用 mc.model（SRT），法线使用 inverse-transpose(3x3)。
 * - 若 baseColorPath 非空，写入 .mtl 的 map_Kd。
 */

// 列优先 4x4 矩阵 * 位置（w=1）
static inline void transformPosition(const float M[16], const float in[3], float out[3])
{
    out[0] = M[0]*in[0] + M[4]*in[1] + M[8] *in[2] + M[12];
    out[1] = M[1]*in[0] + M[5]*in[1] + M[9] *in[2] + M[13];
    out[2] = M[2]*in[0] + M[6]*in[1] + M[10]*in[2] + M[14];
}

// 列优先 3x3 矩阵 * 向量
static inline void mulMat3Vec3(const float M3[9], const float in[3], float out[3])
{
    out[0] = M3[0]*in[0] + M3[3]*in[1] + M3[6]*in[2];
    out[1] = M3[1]*in[0] + M3[4]*in[1] + M3[7]*in[2];
    out[2] = M3[2]*in[0] + M3[5]*in[1] + M3[8]*in[2];
}

// 从 4x4 计算法线矩阵（inverse-transpose 的左上 3x3），列优先写入 out3x3[9]
static inline void computeNormalMat3(const float M[16], float out3x3[9])
{
    float inv[16];  bx::mtxInverse(inv, M);
    float n4[16];   bx::mtxTranspose(n4, inv);
    // 左上 3x3（列优先）
    out3x3[0] = n4[0];  out3x3[1] = n4[1];  out3x3[2] = n4[2];
    out3x3[3] = n4[4];  out3x3[4] = n4[5];  out3x3[5] = n4[6];
    out3x3[6] = n4[8];  out3x3[7] = n4[9];  out3x3[8] = n4[10];
}

static inline float length3(const float v[3])
{
    return std::sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

// —— 读取导出缓存：优先用 CPU 数据，否则输出占位三角 —— //
static void fetchMeshCPUData(
    const MeshComp& mc,
    std::vector<MeshComp::MeshVertexExport>& outVerts,
    std::vector<uint32_t>& outIndices)
{
    outVerts.clear();
    outIndices.clear();

    if (!mc.cpuVertices.empty() && !mc.cpuIndices.empty())
    {
        outVerts   = mc.cpuVertices;  // 类型完全一致：std::vector<MeshComp::MeshVertexExport>
        outIndices = mc.cpuIndices;
        return;
    }

    // 占位（保证流程可跑）
    outVerts.push_back({0,0,0, 0,0,1, 0,0});
    outVerts.push_back({1,0,0, 0,0,1, 1,0});
    outVerts.push_back({0,1,0, 0,0,1, 0,1});
    outIndices = {0,1,2};
}

bool exportSceneToOBJ(const Scene& scn, const std::string& objPath, const std::string& mtlName)
{
    if (scn.meshes.empty()) {
        spdlog::warn("[Exporter] Scene is empty. Nothing to export.");
        return false;
    }

    // 确保导出目录存在
    fs::path outObj = fs::u8path(objPath);
    fs::path outDir = outObj.parent_path();
    std::error_code ec;
    if (!outDir.empty() && !fs::exists(outDir)) {
        if (!fs::create_directories(outDir, ec) && ec) {
            spdlog::error("[Exporter] create_directories failed: {}  ec={}", outDir.string(), ec.message());
            return false;
        }
    }

    // 打开 OBJ / MTL
    std::ofstream ofs(outObj, std::ios::out | std::ios::trunc);
    if (!ofs) {
        spdlog::error("[Exporter] Cannot open OBJ file: {}", outObj.string());
        return false;
    }
    fs::path outMtl = outDir / fs::u8path(mtlName);
    std::ofstream mtl(outMtl, std::ios::out | std::ios::trunc);
    if (!mtl) {
        spdlog::warn("[Exporter] Cannot open MTL file: {}", outMtl.string());
    }

    // 头
    ofs << "# Exported by K-Engine (Day4)\n";
    ofs << "mtllib " << mtlName << "\n";

    uint32_t baseIndex = 1; // OBJ 索引从 1
    int meshId = 0;

    for (auto const& mc : scn.meshes)
    {
        std::vector<MeshComp::MeshVertexExport> verts;
        std::vector<uint32_t> indices;
        fetchMeshCPUData(mc, verts, indices);

        if (verts.empty() || indices.size() < 3) {
            ++meshId;
            continue;
        }

        // 法线矩阵
        float n3[9];
        computeNormalMat3(mc.model, n3);

        // 材质
        std::string matName = "mat_" + std::to_string(meshId);
        if (mtl) {
            mtl << "newmtl " << matName << "\n";
            mtl << "Kd 1.000 1.000 1.000\n";
            mtl << "Ka 0.000 0.000 0.000\n";
            mtl << "Ks 0.000 0.000 0.000\n";
            if (!mc.baseColorPath.empty()) {
                mtl << "map_Kd " << mc.baseColorPath << "\n";
            }
            mtl << "\n";
        }

        ofs << "o mesh_" << meshId << "\n";
        ofs << "usemtl " << matName << "\n";

        // 写位置（已应用模型矩阵）
        for (auto const& v : verts) {
            float pIn[3] = { v.px, v.py, v.pz };
            float pOut[3];
            transformPosition(mc.model, pIn, pOut);
            ofs << "v "  << pOut[0] << " " << pOut[1] << " " << pOut[2] << "\n";
        }

        // 写法线（逆转置 3x3）
        for (auto const& v : verts) {
            float nIn[3] = { v.nx, v.ny, v.nz };
            float nOut[3];
            mulMat3Vec3(n3, nIn, nOut);
            const float len = length3(nOut);
            if (len > 1e-8f) {
                nOut[0] /= len; nOut[1] /= len; nOut[2] /= len;
            }
            ofs << "vn " << nOut[0] << " " << nOut[1] << " " << nOut[2] << "\n";
        }

        // 写 UV（Blender 常用 V 翻转）
        for (auto const& v : verts) {
            ofs << "vt " << v.u << " " << (1.0f - v.v) << "\n";
        }

        // 面（按三角输出）：f v/t/n
        const size_t triCount = indices.size() / 3;
        for (size_t t = 0; t < triCount; ++t) {
            uint32_t i0 = baseIndex + indices[t*3 + 0];
            uint32_t i1 = baseIndex + indices[t*3 + 1];
            uint32_t i2 = baseIndex + indices[t*3 + 2];
            ofs << "f "
                << i0 << "/" << i0 << "/" << i0 << " "
                << i1 << "/" << i1 << "/" << i1 << " "
                << i2 << "/" << i2 << "/" << i2 << "\n";
        }

        baseIndex += static_cast<uint32_t>(verts.size());
        ++meshId;
    }

    spdlog::info("[Exporter] OBJ exported: {}", outObj.string());
    if (mtl) spdlog::info("[Exporter] MTL exported: {}", outMtl.string());
    return true;
}
