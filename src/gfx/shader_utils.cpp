#include "shader_utils.h"

#include <SDL.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

static std::string u8(const fs::path &p)
{
#if defined(_WIN32)
    // C++20 起 u8string() 返回 std::u8string，需要手动拷贝成 std::string
#if defined(__cpp_char8_t)                    // 有 char8_t（C++20）
    auto s8 = p.u8string();                   // std::u8string
    return std::string(s8.begin(), s8.end()); // 逐字节拷贝 UTF-8
#else
    return p.u8string(); // 老编译器直接就是 std::string
#endif
#else
    // POSIX 下 p.string() 默认就是 UTF-8
    return p.string();
#endif
}

std::string ke_resolveShaderPath(const std::string &filename)
{
    std::error_code ec;

    fs::path in(filename);

    // 1) 调用者给了绝对路径且存在
    if (in.is_absolute() && fs::exists(in, ec))
    {
        return u8(in);
    }

    // 2) exeDir/shaders/dx11/<filename>
    char *base = SDL_GetBasePath();
    fs::path exeDir = base ? fs::path(base) : fs::current_path();
    if (base)
        SDL_free(base);

    fs::path p1 = exeDir / "shaders" / "dx11" / in.filename();
    if (fs::exists(p1, ec))
    {
        return u8(p1);
    }

    // 3) 兜底到 CMake 定义的 KE_SHADER_DIR（绝对路径）
#ifdef KE_SHADER_DIR
    fs::path p2 = fs::path(KE_SHADER_DIR) / "dx11" / in.filename();
    if (fs::exists(p2, ec))
    {
        return u8(p2);
    }
#endif

    // 4) 最后尝试相对路径
    if (fs::exists(in, ec))
    {
        return u8(in);
    }

    return {};
}

bgfx::ShaderHandle ke_loadShaderFile(const std::string &filename)
{
    const std::string path = ke_resolveShaderPath(filename);
    if (path.empty())
    {
        spdlog::error("Shader file NOT found: {}", filename);
        return BGFX_INVALID_HANDLE;
    }

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
    {
        spdlog::error("Failed to open shader: {}", path);
        return BGFX_INVALID_HANDLE;
    }

    std::vector<char> buf((std::istreambuf_iterator<char>(ifs)),
                          std::istreambuf_iterator<char>());
    if (buf.empty())
    {
        spdlog::error("Shader file is empty: {}", path);
        return BGFX_INVALID_HANDLE;
    }

    const bgfx::Memory *mem = bgfx::copy(buf.data(), (uint32_t)buf.size());
    bgfx::ShaderHandle h = bgfx::createShader(mem);
    if (!bgfx::isValid(h))
    {
        spdlog::error("createShader failed: {}", path);
    }
    else
    {
        spdlog::info("Loaded shader: {}", path);
    }
    return h;
}

bgfx::ProgramHandle ke_loadProgramDx11(const std::string &vsFilename,
                                       const std::string &fsFilename)
{
    auto vsh = ke_loadShaderFile(vsFilename);
    auto fsh = ke_loadShaderFile(fsFilename);

    if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh))
    {
        spdlog::error("loadProgramDx11 failed. vs='{}' fs='{}'", vsFilename, fsFilename);
        if (bgfx::isValid(vsh))
            bgfx::destroy(vsh);
        if (bgfx::isValid(fsh))
            bgfx::destroy(fsh);
        return BGFX_INVALID_HANDLE;
    }

    auto prog = bgfx::createProgram(vsh, fsh, /*destroyShaders*/ true);
    if (!bgfx::isValid(prog))
    {
        spdlog::error("createProgram failed.");
    }
    return prog;
}
