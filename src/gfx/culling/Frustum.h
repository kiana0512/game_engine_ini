#pragma once
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

struct AABB { glm::vec3 min, max; };

// 名称速记：从 VP 矩阵生成 6 个平面，测试 AABB 是否在视锥里（明天补实现）
struct Frustum {
    void fromMatrix(const glm::mat4& vp);
    bool visible(const AABB& box) const;
};
