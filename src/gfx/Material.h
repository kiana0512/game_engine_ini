#pragma once
#include <bgfx/bgfx.h>
#include <string>
struct Material {
    bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle baseColor = BGFX_INVALID_HANDLE;
    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                   | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW;
    bool useTexture = false;
};
