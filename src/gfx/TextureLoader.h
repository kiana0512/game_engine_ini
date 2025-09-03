#pragma once
#include <bgfx/bgfx.h>
#include <string>
#include <vector>

// 读文件到 RGBA8 像素；flipY=true 时做“上下翻转”（多数 2D 图片的原点在左上角）
bool loadImageRGBA(const std::string& path, int& w, int& h,
                   std::vector<uint8_t>& pixels, bool flipY = true);

// 直接创建 bgfx 纹理；失败返回 BGFX_INVALID_HANDLE
bgfx::TextureHandle createTexture2DFromFile(const std::string& path,
                                            bool srgb = false,
                                            uint64_t samplerFlags = 0);
