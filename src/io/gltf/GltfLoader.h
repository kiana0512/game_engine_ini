#pragma once
#include <string>
#include <vector>

// 顶点结构（位置/法线/UV）
struct MeshVertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<uint16_t>   indices;
    // BaseColor 贴图路径（如果 glTF 使用了贴图且是外链）
    std::string baseColorTexPath;
};

// 读取 .gltf / .glb；仅取第一个 mesh primitive（TRIANGLES）
bool loadGltfMesh(const std::string& path, MeshData& out,
                  std::string* warn = nullptr, std::string* err = nullptr);
