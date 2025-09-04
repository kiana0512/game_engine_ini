#pragma once
#include <bgfx/bgfx.h>
#include <string>
#include <unordered_map>

class ResourceCache {
public:
    bgfx::TextureHandle getTexture2D(const std::string& path,
                                     bool flipY = true);
    void clear(); // 退出时释放（你已在 Renderer::shutdown 释放全局）
private:
    std::unordered_map<std::string, bgfx::TextureHandle> texCache_;
};
