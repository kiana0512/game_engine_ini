#pragma once                      // 头文件只包含一次的编译器指示（比传统 include guard 更简洁）

#include <bgfx/bgfx.h>            // 引入 bgfx 基础 API（setViewTransform、getCaps 等）
#include <bx/math.h>              // 引入 bx 的数学工具（mtxLookAt/mtxProj/mtxOrtho、memCopy 等）

// ===============================
// 简单相机：支持正交/透视两种模式；允许外部注入 view/proj 矩阵
// 语法点：enum class 是“作用域枚举”，不会把枚举名泄漏到全局命名空间，比传统 enum 更安全。
// ===============================
enum class CameraMode { Ortho, Perspective };

// ===============================
// Camera 类：内部维护两块 4×4 float 矩阵（列主序），以及视口尺寸与模式标记。
// 我们将新增两个 **只读 getter**：viewPtr()/projPtr() —— 解决 ForwardPBRPass 想要拿指针的问题。
// ===============================
class Camera {
public:
    Camera() = default;           // 默认构造函数（不做额外工作，成员用类内默认初始化）

    // ---------------------------
    // 切换相机模式（只影响投影矩阵）
    // ---------------------------
    void setMode(CameraMode m) {
        mode_ = m;                // 赋值：记录新的相机模式（透视/正交）
        useExternalProj_ = false; // 置为 false：表示“不要使用外部投影矩阵”，下次 apply() 重新按模式生成
    }

    // 只读获取当前相机模式（const 成员函数：承诺不改写任何成员；返回按值拷贝一个枚举）
    CameraMode mode() const { return mode_; }

    // ---------------------------
    // 更新视口（只影响投影矩阵的宽高比）
    // ---------------------------
    void setViewport(int w, int h) {
        width_ = w;               // 记录视口宽
        height_ = h;              // 记录视口高
        useExternalProj_ = false; // 变更了宽高比：清除外部投影标记，提示下次 apply() 需要重建投影
    }

    // ---------------------------
    // 由外部（例如 CameraController）直接设置视图矩阵
    // 语法点：参数是 const float*，代表“指向只读 float”的裸指针；我们会把 16 个 float 拷贝进内部缓冲。
    // ---------------------------
    void setView(const float* viewMtx) {
        bx::memCopy(view_, viewMtx, sizeof(float)*16); // 拷贝 16 个 float（列主序），避免悬垂指针
        useExternalView_ = true;    // 标为 true：apply() 不再重建 view，采用外部注入
    }

    // ---------------------------
    // 可选：外部直接设置投影矩阵
    // ---------------------------
    void setProj(const float* projMtx) {
        bx::memCopy(proj_, projMtx, sizeof(float)*16); // 同理，拷贝 16 个 float
        useExternalProj_ = true;    // 标为 true：apply() 不再重建 proj，采用外部注入
    }

    // =========================================================
    //  新增（修复 ForwardPBRPass 报错的关键）：
    // 只读获取内部存储的 4×4 视图/投影矩阵“首元素指针”（列主序，连续 16 个 float）
    // 语法点：
    // - 返回类型 const float*：调用方只能读，不能通过指针改写内部数据。
    // - 成员函数后缀 const：保证不修改任何成员（包括矩阵内容与标志位）。
    // - noexcept：承诺不抛异常，利于编译器优化与异常安全分析。
    // =========================================================
    const float* viewPtr() const noexcept { return view_; }
    const float* projPtr() const noexcept { return proj_; }

    // ---------------------------
    // 把 view/proj 应用到指定的 ViewId
    // ---------------------------
    void apply(bgfx::ViewId viewId) {
        // 仅对需要的部分重建，避免覆盖外部注入的矩阵
        if (!useExternalView_)  rebuildView_();  // 如果没有“外部 view”，就按内置规则重建一次
        if (!useExternalProj_)  rebuildProj_();  // 如果没有“外部 proj”，就按模式/视口重建一次

        // bgfx 期望的是列主序的 4×4 float 数组指针（各 16 个 float）
        bgfx::setViewTransform(viewId, view_, proj_);
    }

private:
    // ---------------------------
    // 只重建视图矩阵（默认固定在世界原点看向 +Z，可按需修改）
    // ---------------------------
    void rebuildView_() {
        // 这里给一个“可看的默认相机”——从 (0,0,-2.2) 看向原点，y 轴向上，右手坐标系
        const bx::Vec3 eye = {0.0f, 0.0f, -2.2f};               // 相机位置（世界空间）
        const bx::Vec3 at  = {0.0f, 0.0f,  0.0f};               // 注视点（世界空间）
        const bx::Vec3 up  = {0.0f, 1.0f,  0.0f};               // 上方向（世界空间）
        bx::mtxLookAt(
            view_,                                              // 输出：4×4 视图矩阵（列主序）
            eye, at, up,                                        // 输入：眼/看点/上方向
            bx::Handedness::Right                               // 指定右手系（与 bgfx 默认一致）
        );
        // 注意：这里不把 useExternalView_ 置 true —— 语义上它仍然是“内部维护”的 view
    }

    // ---------------------------
    // 只重建投影矩阵（根据模式与视口）
    // ---------------------------
    void rebuildProj_() {
        if (mode_ == CameraMode::Ortho) {
            // 正交相机：这里用 NDC 区域 [-1,1] 的单位盒；近远裁剪设成 [0, 100]
            // 第 7 个参数传 0.0f 说明使用默认的反向 Y（bgfx 根据后端处理），
            // 最后一个参数指定是否使用齐次深度（-1..1），跟随 bgfx 能力集。
            bx::mtxOrtho(
                proj_,                  // 输出：4×4 投影矩阵
                -1.0f, 1.0f,            // 左右
                -1.0f, 1.0f,            // 下上
                0.0f, 100.0f,           // 近平面/远平面
                0.0f,                   // 额外参数（通常填 0）
                bgfx::getCaps()->homogeneousDepth  // 是否齐次深度
            );
        } else {
            // 透视相机：设定纵向 FOV 和宽高比，从而生成典型的透视投影
            const float fovY = 60.0f;                                  // 纵向视场角（度）
            const float aspect = (height_ > 0)                         // 宽高比（避免被 0 除）
                                ? float(width_) / float(height_)
                                : 16.0f / 9.0f;
            bx::mtxProj(
                proj_,                  // 输出：4×4 投影矩阵
                fovY,                   // 纵向 FOV（单位：度）
                aspect,                 // 宽高比（w/h）
                0.1f, 100.0f,           // 近平面/远平面
                bgfx::getCaps()->homogeneousDepth  // 是否齐次深度
            );
        }
        // 这里不修改 useExternalProj_，保持“内部投影由相机自己维护”的语义
    }

private:
    // ============ 成员数据（类内初始化）============
    CameraMode mode_ = CameraMode::Perspective; // 当前相机模式（默认透视）
    int   width_ = 1280, height_ = 720;         // 视口尺寸（默认 1280×720）

    // 标记开关：是否使用“外部注入”的矩阵
    // 语义：true = 外部已注入（setView/setProj 调用过），apply() 不会重建对应矩阵；
    //      false = 使用内部规则重建（rebuildView_/rebuildProj_）
    bool  useExternalView_ = false;
    bool  useExternalProj_ = false;

    // 4×4 矩阵缓冲（列主序，连续 16 个 float）
    float view_[16]{};                          // 用 {} 零初始化（C++11 列表初始化）
    float proj_[16]{};
};
