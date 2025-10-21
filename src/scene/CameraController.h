#pragma once                                  // 头文件只编译一次（比传统 include guard 简洁）

#include "../gfx/camera/Camera.h"             // 引入相机类（控制器会把 view 矩阵写回相机）
#include "Input.h"                            // 引入输入系统（键鼠查询）
#include <bx/math.h>                          // bx 数学库（向量/矩阵/三角函数等）

// 简易自由相机控制器（右键拖拽旋转 + WASD/空格/C 平移 + Shift 冲刺）
// 设计要点：控制器维护“姿态”（位置/朝向），每帧在 Update() 中把 view 矩阵写回 Camera。
class CameraController
{
public:                                       // 公有区域：对外可见的类型与成员函数
    // 运行参数（移动速度/鼠标灵敏度等） —— 使用“聚合体”便于字面量初始化
    struct Params {
        float moveSpeed = 5.0f;               // 基础移动速度（米/秒）
        float sprintMul = 2.5f;               // 冲刺倍速（按住 Shift）
        float mouseSens = 0.0025f;            // 鼠标灵敏度（弧度/像素）
        float pitchLimit = 1.55f;             // 俯仰角限制（约 ±88.8°，防止翻转）
    };

    // 构造：持有外部 Camera 与 Input 的“非拥有”指针（生命周期由外部管理）
    // explicit 防止发生隐式转换；两个参数都不可为空（在 .cpp 可断言）
    explicit CameraController(Camera* cam, Input* input);

    // 设置/获取参数：传值接收一个 Params 副本；Get 返回 const 引用避免拷贝
    void SetParams(const Params& p) { m_params = p; }     // 赋值更新运行参数
    const Params& GetParams() const { return m_params; }  // 只读访问当前参数

    // 每帧调用（dt：帧时间，秒）。内部会读取输入、更新 m_pos/m_yaw/m_pitch，并写回 Camera::setView()
    void Update(float dt);

    // 可选：一次性设置初始姿态（位置 + 朝向）。单位：位置（世界坐标），角度为“弧度”。
    void SetPose(const bx::Vec3& pos, float yaw, float pitch);

    // ===================== 新增 Getter（关键补丁）=====================
    // 只读地获取相机“世界空间位置” —— 供 Renderer 组装 RenderContext 或设置 u_CamPos_Time
    const bx::Vec3& GetPosition() const noexcept { return m_pos; }  // noexcept：承诺不抛异常

    // （可选）调试用途：读取当前水平角/俯仰角 —— 便于 UI 显示或保存相机状态
    float GetYaw()   const noexcept { return m_yaw; }               // 返回当前 yaw（弧度）
    float GetPitch() const noexcept { return m_pitch; }             // 返回当前 pitch（弧度）
    // ===================== 新增 Getter 结束 ==========================

private:                                      // 私有区域：仅类内部可见的数据成员
    Camera* m_cam = nullptr;                  // 相机指针（非拥有）。Update() 会对它调用 setView()
    Input*  m_input = nullptr;                // 输入系统指针（非拥有）。用于读取键鼠

    Params m_params{};                        // 一份可变的参数副本（默认值见上面的聚合体初始化）

    // 相机“姿态”状态（右手系）：
    bx::Vec3 m_pos { 0.0f, 1.6f, 3.0f };      // 世界空间位置（默认略高于地面，朝向原点）
    float    m_yaw   = 0.0f;                  // 水平角（弧度，绕世界 +Y 旋转）
    float    m_pitch = 0.0f;                  // 俯仰角（弧度，绕相机右向量旋转，范围受 pitchLimit 约束）
};
