#include "TextureLoader.h"
#include <spdlog/spdlog.h>

// 只需要这一个头；实现放在本 cpp（单 TU）里
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION   // 新增：提供 stbi_write_* 的实现
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_MSC_SECURE_CRT
#include <stb_image.h>
#include <stb_image_write.h>

bool loadImageRGBA(const std::string& path, int& w, int& h,
                   std::vector<uint8_t>& pixels, bool flipY)
{
    stbi_set_flip_vertically_on_load(flipY ? 1 : 0);

    int comp = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, STBI_rgb_alpha);
    if (!data) {
        spdlog::error("[Texture] load failed: {} ({})", path, stbi_failure_reason());
        return false;
    }

    size_t size = size_t(w) * size_t(h) * 4;
    pixels.assign(data, data + size);
    stbi_image_free(data);
    return true;
}

bgfx::TextureHandle createTexture2DFromFile(const std::string& path,
                                            bool /*srgb*/,
                                            uint64_t /*samplerFlags*/)
{
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;
    if (!loadImageRGBA(path, w, h, rgba, /*flipY=*/true)) {
        return BGFX_INVALID_HANDLE;
    }

    const bgfx::Memory* mem = bgfx::copy(rgba.data(), (uint32_t)rgba.size());
    // 这里只建 base level；需要 mip 的话后面我们再加离线/在线生成
    auto tex = bgfx::createTexture2D((uint16_t)w, (uint16_t)h,
                                     false, 1, bgfx::TextureFormat::RGBA8, 0, mem);
    if (!bgfx::isValid(tex)) {
        spdlog::error("[Texture] createTexture2D failed: {}", path);
    }
    return tex;
}
