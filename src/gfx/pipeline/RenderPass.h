#pragma once  // 确保本头文件只会被编译器包含一次（比 #ifndef 宏更简洁）

#include <cstdint>    // 固定宽度整数类型（uint64_t 等）
#include <vector>     // std::vector 动态数组容器
#include <memory>     // std::unique_ptr 智能指针
#include <bgfx/bgfx.h> // bgfx 句柄与渲染 API（ViewId、VertexBufferHandle 等）

// =============================
// DrawKey：排序键（64 位）
// 用“位域”把一个 64-bit 整数分成多个子字段，
// 排序时只比较一个整数，减少分支/加速缓存友好。
// =============================
union DrawKey {
    uint64_t value; // 作为整体整数使用（比较排序更快）

    struct {
        uint64_t pipeline : 8;  // 渲染管线/Pass 类型（例如 ForwardPBR=1）
        uint64_t material : 24; // 材质或程序 ID（减少频繁切换 program/纹理）
        uint64_t depth    : 32; // 近似深度（用于前向渲染的前后排序/减少 overdraw）
    } bits;

    // constexpr 构造：允许常量表达式初始化；noexcept：承诺不抛异常
    constexpr DrawKey(uint64_t v = 0) noexcept : value(v) {}
};

// 便捷工厂：把 3 个字段打包成 DrawKey；inline：多处包含也不会重复定义实体
inline DrawKey makeDrawKey(uint8_t pipeline, uint32_t material, uint32_t depth) noexcept {
    DrawKey k{};                 // 聚合初始化为 0
    k.bits.pipeline = pipeline;  // 写入各字段
    k.bits.material = material;
    k.bits.depth    = depth;
    return k;                    // 返回值优化（NRVO），无拷贝成本
}

// =============================
// DrawItem：一次可提交的绘制命令的最小数据
// =============================
struct DrawItem final {                          // final：禁止继续被继承
    DrawKey key;                                 // 排序键（决定提交顺序）
    bgfx::VertexBufferHandle vbh{};              // 顶点缓冲句柄（默认无效句柄）
    bgfx::IndexBufferHandle  ibh{};              // 索引缓冲句柄（可无）
    uint32_t                 numIndices = 0;     // 绘制索引数（0 表示按 vbh 计数）

    float model[16]{};                           // 4x4 模型矩阵（列主序 16 个 float）

    // 三张常见贴图（无贴图则保持 BGFX_INVALID_HANDLE）
    bgfx::TextureHandle baseColorTex{BGFX_INVALID_HANDLE}; // 基础色
    bgfx::TextureHandle normalTex   {BGFX_INVALID_HANDLE}; // 法线
    bgfx::TextureHandle mrTex       {BGFX_INVALID_HANDLE}; // MetallicRoughness（R/G）

    // PBR 参数（最小集合）
    float baseColor[3] = {1.f, 1.f, 1.f};        // 基础色（线性空间）
    float metallic     = 0.0f;                   // 金属度 [0,1]
    float roughness    = 1.0f;                   // 粗糙度 [0,1]

    // 固定状态位（深度测试、写入、剔除、MSAA 等）
    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_Z |
                     BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CCW | BGFX_STATE_MSAA;
};

// =============================
// RenderContext：传给各个 Pass 的“只读帧数据”
// 重要：我们不再传 Camera 对象，只传纯数据：view/proj 指针 + 相机位置。
// 这样 Pass 与 Camera 的具体接口完全解耦，避免 C2039 这类成员找不到的问题。
// =============================
struct RenderContext final {
    bgfx::ViewId view;                                // 渲染视图 ID（0..255）
    const float* viewMatrix;                          // 视图矩阵指针（列主序 16 float）
    const float* projMatrix;                          // 投影矩阵指针（列主序 16 float）
    float        camPos[3];                           // 摄像机世界空间坐标 (x,y,z)
    float        timeSec;                             // 运行时间（秒）
    const std::vector<DrawItem>& drawList;            // 本帧的绘制条目列表（只读引用）
};

// =============================
// RenderPass：渲染通道抽象基类
// - prepare()：可选，做懒加载或本帧预处理
// - execute()：必须，实现绘制提交
// 禁拷贝：避免资源拥有者被意外复制
// =============================
class RenderPass {
public:
    virtual ~RenderPass() = default;                 // 虚析构：通过基类指针 delete 派生类安全

    virtual void prepare(const RenderContext& /*rc*/) {} // 默认空实现（可不重载）

    virtual void execute(const RenderContext& rc) = 0;   // 纯虚函数：派生类必须实现

    RenderPass(const RenderPass&) = delete;              // 禁用拷贝构造
    RenderPass& operator=(const RenderPass&) = delete;   // 禁用拷贝赋值

protected:
    RenderPass() = default;                              // 受保护的默认构造（只能被派生类调用）
};
