#include <SDL.h>
#include <SDL_syswm.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/math.h>
#include <spdlog/spdlog.h>
#include "gfx/shader_utils.h"

// 取本机窗口句柄给 bgfx
static void *sdlNativeWindowHandle(SDL_Window *win)
{
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (SDL_GetWindowWMInfo(win, &wmi) != SDL_TRUE)
        return nullptr;
#if defined(_WIN32)
    return wmi.info.win.window;
#elif defined(__APPLE__)
    return wmi.info.cocoa.window;
#else
    return (void *)(uintptr_t)wmi.info.x11.window;
#endif
}

// 顶点：位置(float3) + 颜色(RGBA8 归一化)
struct PosColorVertex
{
    float x, y, z;
    uint32_t abgr;
    static void init(bgfx::VertexLayout &layout)
    {
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true) // normalized
            .end();
    }
};

int main(int, char **)
{
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        spdlog::error("SDL_Init failed: {}", SDL_GetError());
        return -1;
    }

    int width = 1280, height = 720;
    SDL_Window *window = SDL_CreateWindow(
        "K-Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window)
    {
        spdlog::error("SDL_CreateWindow failed: {}", SDL_GetError());
        return -1;
    }

    // ===== 初始化 bgfx =====
    bgfx::Init init{};
    init.type = bgfx::RendererType::Count; // 自动选择后端
    init.platformData.nwh = sdlNativeWindowHandle(window);
    init.resolution.width = (uint32_t)width;
    init.resolution.height = (uint32_t)height;
    init.resolution.reset = BGFX_RESET_VSYNC;
    if (!bgfx::init(init))
    {
        spdlog::error("bgfx init failed");
        return -1;
    }

    bgfx::setDebug(BGFX_DEBUG_TEXT | BGFX_DEBUG_IFH | BGFX_DEBUG_STATS);

    // 我们用 View=1（View 0 给调试叠层）
    const bgfx::ViewId kView = 1;
    bgfx::setViewName(kView, "main");
    bgfx::setViewClear(kView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
    bgfx::setViewRect(kView, 0, 0, (uint16_t)width, (uint16_t)height);
    bgfx::touch(kView);

    // ===== 顶点数据（非索引绘制）=====
    bgfx::VertexLayout layout;
    PosColorVertex::init(layout);

    // 3 个顶点（无 IBO）
    const PosColorVertex s_verts[] = {
        {0.0f, 0.6f, 0.0f, 0xff00ffff},  // 紫青
        {0.6f, -0.6f, 0.0f, 0xffffff00}, // 黄
        {-0.6f, -0.6f, 0.0f, 0xff00ff00} // 绿
    };

    bgfx::VertexBufferHandle vbh =
        bgfx::createVertexBuffer(bgfx::copy(s_verts, sizeof(s_verts)), layout);
    if (!bgfx::isValid(vbh))
    {
        spdlog::error("VBO invalid.");
        return -1;
    }

    // ===== 新增：索引数据 + 索引缓冲 =====
    // 用 16-bit 索引（bgfx 默认）
    const uint16_t s_indices[] = {0, 1, 2}; // 一个三角形
    bgfx::IndexBufferHandle ibh =
        bgfx::createIndexBuffer(bgfx::copy(s_indices, sizeof(s_indices)));
    if (!bgfx::isValid(ibh))
    {
        spdlog::error("IBO invalid.");
        return -1;
    }
    // ===== 着色器程序 =====
    spdlog::info("Loading shaders (vs_simple.bin / fs_simple.bin) ...");
    bgfx::ProgramHandle program = ke_loadProgramDx11("vs_simple.bin", "fs_simple.bin");
    if (!bgfx::isValid(program))
    {
        spdlog::error("Failed to load shaders.");
        return -1;
    }

    // ===== 相机（正交）=====
    float view[16];
    bx::mtxIdentity(view);
    float proj[16];
    bx::mtxOrtho(
        proj, -1.0f, 1.0f, -1.0f, 1.0f,
        0.0f, 100.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(kView, view, proj);

    // 用于显示“上一帧统计”的缓存
    uint32_t lastNumDraw = 0, lastTris = 0;

    float angle = 0.0f;
    uint32_t lastTicks = SDL_GetTicks();
    bool running = true;

    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                running = false;
            else if (e.type == SDL_WINDOWEVENT &&
                     (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                      e.window.event == SDL_WINDOWEVENT_RESIZED))
            {
                width = e.window.data1;
                height = e.window.data2;
                bgfx::reset((uint32_t)width, (uint32_t)height, BGFX_RESET_VSYNC);
                bgfx::setViewRect(kView, 0, 0, (uint16_t)width, (uint16_t)height);
                bx::mtxOrtho(proj, -1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 100.0f, 0.0f,
                             bgfx::getCaps()->homogeneousDepth);
                bgfx::setViewTransform(kView, view, proj);
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_r)
            {
                if (bgfx::isValid(program))
                    bgfx::destroy(program);
                program = ke_loadProgramDx11("vs_simple.bin", "fs_simple.bin");
            }
        }

        uint32_t now = SDL_GetTicks();
        float dt = (now - lastTicks) * 0.001f;
        lastTicks = now;
        angle += dt;

        // ===== 在本帧开头显示“上一帧”的统计，确保你能看到 =====
        bgfx::dbgTextClear();
        bgfx::dbgTextPrintf(0, 0, 0x0f, "Draw(last)=%u  Tris(last)=%u", lastNumDraw, lastTris);
        bgfx::dbgTextPrintf(0, 1, 0x0a, "Press R to reload shaders");

        // 每帧设置 VP（有些后端 reset 后需要重新设）
        bgfx::setViewTransform(kView, view, proj);
        bgfx::touch(kView);

        // 模型旋转
        float model[16];
        bx::mtxRotateZ(model, angle);

        // 绑定资源（索引绘制）
        bgfx::setTransform(model);
        bgfx::setVertexBuffer(0, vbh); //  只设置一次
        bgfx::setIndexBuffer(ibh);     //  用 IBO，三角形 3 个索引

        // 渲染状态（不剔除/不深度，保证可见）
        const uint64_t kState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA;
        bgfx::setState(kState);

        bgfx::submit(kView, program);

        // 结束本帧
        bgfx::frame();

        // 取到“本帧”的统计，缓存起来供下一帧显示
        if (const bgfx::Stats *st = bgfx::getStats())
        {
            lastNumDraw = st->numDraw;
#if defined(BGFX_TOPOLOGY_TRI_LIST)
            lastTris = st->numPrims[BGFX_TOPOLOGY_TRI_LIST];
#else
            lastTris = st->numPrims[(uint8_t)bgfx::Topology::TriList];
#endif
        }
    }

    bgfx::destroy(vbh);
    bgfx::destroy(program);
    bgfx::shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
