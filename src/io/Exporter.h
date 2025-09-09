#pragma once
#include <string>
#include "scene/Scene.h"

/**
 * 导出场景为 Wavefront OBJ（附 .mtl）
 * - 支持：位置(v)、法线(vn)、UV(vt)、三角面(f)
 * - 贴图：为每个 mesh 写入一个 material，若有 baseColor 纹理则写入 map_Kd
 * - 注意：OBJ 没有节点层级与单位，默认世界坐标
 * 返回：true 成功 / false 失败
 */
bool exportSceneToOBJ(const Scene& scn, const std::string& objPath, const std::string& mtlName = "materials.mtl");
