#pragma once
#include "../gfx/Camera.h"
#include "Input.h"
#include <bx/math.h>

// 简易自由相机控制器（右键拖拽旋转 + WASD/空格/C 平移 + Shift 冲刺）
class CameraController
{
public:
    struct Params {
        float moveSpeed = 5.0f;      // 基础移动速度（米/秒）
        float sprintMul = 2.5f;      // 冲刺倍速
        float mouseSens = 0.0025f;   // 鼠标灵敏度（弧度/像素）
        float pitchLimit = 1.55f;    // 抬头/低头最大角（~88.8°）
    };

    CameraController(Camera* cam, Input* input);

    void SetParams(const Params& p) { m_params = p; }
    const Params& GetParams() const { return m_params; }

    // 每帧调用
    void Update(float dt);

    // 可选：初始化位置/朝向
    void SetPose(const bx::Vec3& pos, float yaw, float pitch);

private:
    Camera* m_cam = nullptr;
    Input*  m_input = nullptr;

    Params m_params;

    // 相机姿态（右手系，yaw 绕世界 Up[Y]，pitch 绕右向量）
    bx::Vec3 m_pos { 0.0f, 1.6f, 3.0f };
    float m_yaw   = 0.0f; // 水平角（弧度）
    float m_pitch = 0.0f; // 俯仰角（弧度）
};
