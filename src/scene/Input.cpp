#include "Input.h"
#include <algorithm>

static inline bool keyIsRepeat(const SDL_KeyboardEvent& ke) {
    return ke.repeat != 0;
}

void Input::BeginFrame() {
    // 清理“瞬时触发”状态；鼠标增量清零
    for (auto& kv : m_pressed) kv.second = false;
    m_mouseDx = 0;
    m_mouseDy = 0;
}

void Input::EndFrame() {
    // 目前无需处理；保留扩展
}

bool Input::IsDown(Action a) const {
    auto it = m_down.find(a);
    return it != m_down.end() && it->second;
}

bool Input::WasPressed(Action a) const {
    auto it = m_pressed.find(a);
    return it != m_pressed.end() && it->second;
}

void Input::setDown(Action a, bool down) {
    bool old = IsDown(a);
    m_down[a] = down;
    if (!old && down) {
        m_pressed[a] = true; // 本帧内标记“刚刚被按下”
    }
}

void Input::HandleSDLEvent(const SDL_Event& e) {
    switch (e.type)
    {
    case SDL_KEYDOWN: // SDL2 的事件类型
    {
        const auto& ke = e.key;
        if (keyIsRepeat(ke)) break;
        switch (ke.keysym.sym)
        {
        case SDLK_w: setDown(Action::MoveForward, true); break;
        case SDLK_s: setDown(Action::MoveBackward, true); break;
        case SDLK_a: setDown(Action::StrafeLeft,  true); break;
        case SDLK_d: setDown(Action::StrafeRight, true); break;
        case SDLK_SPACE: setDown(Action::MoveUp,   true); break;
        case SDLK_c:     setDown(Action::MoveDown, true); break;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT: setDown(Action::Sprint, true); break;
        default: break;
        }
    } break;

    case SDL_KEYUP:
    {
        const auto& ke = e.key;
        switch (ke.keysym.sym)
        {
        case SDLK_w: setDown(Action::MoveForward, false); break;
        case SDLK_s: setDown(Action::MoveBackward, false); break;
        case SDLK_a: setDown(Action::StrafeLeft,  false); break;
        case SDLK_d: setDown(Action::StrafeRight, false); break;
        case SDLK_SPACE: setDown(Action::MoveUp,   false); break;
        case SDLK_c:     setDown(Action::MoveDown, false); break;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT: setDown(Action::Sprint, false); break;
        default: break;
        }
    } break;

    case SDL_MOUSEBUTTONDOWN:
        if (e.button.button == SDL_BUTTON_RIGHT) {
            setDown(Action::CameraRotate, true);
            if (!m_relativeMouse) {
                SDL_SetRelativeMouseMode(SDL_TRUE);
                m_relativeMouse = true;
            }
        }
        break;

    case SDL_MOUSEBUTTONUP:
        if (e.button.button == SDL_BUTTON_RIGHT) {
            setDown(Action::CameraRotate, false);
            if (m_relativeMouse) {
                SDL_SetRelativeMouseMode(SDL_FALSE);
                m_relativeMouse = false;
            }
        }
        break;

    case SDL_MOUSEMOTION:
        if (IsDown(Action::CameraRotate)) {
            m_mouseDx += e.motion.xrel;
            m_mouseDy += e.motion.yrel;
        }
        break;

    default: break;
    }
}
