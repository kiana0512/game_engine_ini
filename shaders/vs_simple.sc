$input  a_position, a_color0
$output v_color0

#include <bgfx_shader.sh>

// 注意：这里不要再写 "uniform mat4 u_modelViewProj;"
// bgfx 在 D3D/GL 等后端会预定义该 uniform。

void main()
{
    // 预定义的 u_modelViewProj * 位置 => 裁剪空间坐标
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_color0 = a_color0;  // 传给像素着色器
}
