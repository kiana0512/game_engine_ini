#pragma once
#include <bgfx/bgfx.h>
#include <string>

// 解析并返回可用的着色器路径（绝对路径）。找不到则返回空字符串。
std::string ke_resolveShaderPath(const std::string& filename);

// 加载单个 .bin（filename 可以是“vs_simple.bin”这样的文件名，或绝对/相对路径）
bgfx::ShaderHandle ke_loadShaderFile(const std::string& filename);

// 组合创建 Program（推荐：直接传文件名）
bgfx::ProgramHandle ke_loadProgramDx11(const std::string& vsFilename,
                                       const std::string& fsFilename);
