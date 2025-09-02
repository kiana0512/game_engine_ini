$input v_color0
#include <bgfx_shader.sh>

void main()
{
    // 最简单：直接输出插值过来的颜色
    gl_FragColor = v_color0;
}
