$input v_normal
#include "bgfx_shader.sh"
uniform vec4 u_lightDir; // xyz 方向
void main() {
    vec3 n  = normalize(v_normal);
    vec3 ld = normalize(u_lightDir.xyz);
    float ndl = max(dot(n, ld), 0.0);
    gl_FragColor = vec4(ndl, ndl, ndl, 1.0);
}
