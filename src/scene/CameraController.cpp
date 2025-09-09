#include "CameraController.h"
#include <algorithm>
#include <cmath>

// 一些局部向量工具
static inline bx::Vec3 vadd(const bx::Vec3& a, const bx::Vec3& b) {
    return bx::Vec3{ a.x + b.x, a.y + b.y, a.z + b.z };
}
static inline bx::Vec3 vscale(const bx::Vec3& v, float s) {
    return bx::Vec3{ v.x * s, v.y * s, v.z * s };
}
static inline bx::Vec3 vsub(const bx::Vec3& a, const bx::Vec3& b) {
    return bx::Vec3{ a.x - b.x, a.y - b.y, a.z - b.z };
}

CameraController::CameraController(Camera* cam, Input* input)
    : m_cam(cam), m_input(input) {}

void CameraController::SetPose(const bx::Vec3& pos, float yaw, float pitch) {
    m_pos = pos;
    m_yaw = yaw;
    m_pitch = pitch;
}

void CameraController::Update(float dt) {
    if (!m_cam || !m_input) return;

    // 1) 鼠标右键拖拽：更新 yaw/pitch
    if (m_input->IsDown(Action::CameraRotate)) {
        int dx=0, dy=0;
        m_input->GetMouseDelta(dx, dy);
        m_yaw   -= dx * m_params.mouseSens; // 右移 -> 向右看
        m_pitch -= dy * m_params.mouseSens; // 上移 -> 向上看
        m_pitch = std::clamp(m_pitch, -m_params.pitchLimit, m_params.pitchLimit);
    }

    // 2) 由 yaw/pitch 计算相机前/右/上
    const float cy = std::cos(m_yaw), sy = std::sin(m_yaw);
    const float cp = std::cos(m_pitch), sp = std::sin(m_pitch);

    // 右手系：Z 指向前；与 bgfx/bx 默认一致
    bx::Vec3 forward = { sy * cp, sp, -cy * cp };
    bx::Vec3 right   = {  cy, 0.0f,  sy };
    bx::Vec3 up      = { 0.0f, 1.0f, 0.0f };

    // 3) 键盘：WASD/Space/C + Shift 冲刺
    float speed = m_params.moveSpeed * (m_input->IsDown(Action::Sprint) ? m_params.sprintMul : 1.0f);
    bx::Vec3 move{0,0,0};
    if (m_input->IsDown(Action::MoveForward))  move = vadd(move, forward);
    if (m_input->IsDown(Action::MoveBackward)) move = vsub(move, forward);
    if (m_input->IsDown(Action::StrafeRight))  move = vadd(move, right);
    if (m_input->IsDown(Action::StrafeLeft))   move = vsub(move, right);
    if (m_input->IsDown(Action::MoveUp))       move = vadd(move, up);
    if (m_input->IsDown(Action::MoveDown))     move = vsub(move, up);

    // 归一化 + 位移
    const float lenSq = move.x*move.x + move.y*move.y + move.z*move.z;
    if (lenSq > 1e-6f) {
        move = vscale(move, (1.0f/std::sqrt(lenSq)) * speed * dt);
        m_pos = vadd(m_pos, move);
    }

    // 4) 仅设置“视图矩阵”，投影仍由 Camera 内部根据模式与视口生成
    float view[16];
    bx::mtxLookAt(view, m_pos, vadd(m_pos, forward), up, bx::Handedness::Right);
    m_cam->setView(view);
}
