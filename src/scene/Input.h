#pragma once
#include <SDL.h>
#include <unordered_map>
#include <cstdint>

// 输入动作枚举（可扩展）
enum class Action : uint8_t
{
    MoveForward = 0,
    MoveBackward,
    StrafeLeft,
    StrafeRight,
    MoveUp,
    MoveDown,
    Sprint,
    CameraRotate, // 右键按下即为“旋转相机模式”
};

class Input
{
public:
    Input() = default;

    // 每帧开始/结束用于清理一次性状态（pressed、鼠标增量）
    void BeginFrame();
    void EndFrame();

    // 把 SDL 事件喂进来
    void HandleSDLEvent(const SDL_Event& e);

    // 查询接口
    bool IsDown(Action a) const;
    bool WasPressed(Action a) const;

    // 鼠标移动增量（仅在 CameraRotate 时更新）
    // 注意：返回的是“本帧累计”的增量，调用 BeginFrame() 会清零
    void GetMouseDelta(int& dx, int& dy) const { dx = m_mouseDx; dy = m_mouseDy; }
    bool IsRelativeMouseMode() const { return m_relativeMouse; }

private:
    void setDown(Action a, bool down);

private:
    // 动作状态
    std::unordered_map<Action, bool> m_down;
    std::unordered_map<Action, bool> m_pressed;

    // 鼠标相对模式（右键按下进入）
    bool m_relativeMouse = false;

    // 本帧鼠标移动累计（右键按住时才记录）
    int m_mouseDx = 0;
    int m_mouseDy = 0;
};
