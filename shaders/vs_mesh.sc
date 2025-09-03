$input  a_position, a_normal
$output v_normal
#include "bgfx_shader.sh"
uniform mat4 u_modelViewProj;
void main() {
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_normal    = a_normal;
}
