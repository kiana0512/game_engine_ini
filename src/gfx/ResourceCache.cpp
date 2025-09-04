#include "ResourceCache.h"
#include "TextureLoader.h"  // 你现有的创建函数
#include <bgfx/bgfx.h>

bgfx::TextureHandle ResourceCache::getTexture2D(const std::string& path, bool flipY) {
    auto it = texCache_.find(path);
    if (it != texCache_.end() && bgfx::isValid(it->second)) return it->second;
    auto h = createTexture2DFromFile(path, /*srgb*/false, /*sampler*/0);
    if (bgfx::isValid(h)) texCache_[path] = h;
    return h;
}
void ResourceCache::clear() {
    for (auto& kv : texCache_) if (bgfx::isValid(kv.second)) bgfx::destroy(kv.second);
    texCache_.clear();
}
