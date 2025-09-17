$input  a_position, a_normal, a_texcoord0
$output v_texcoord0, v_worldPos, v_normalWS

#include "bgfx_shader.sh"

void main()
{
    vec4 wpos   = mul(u_model[0], vec4(a_position, 1.0));
    v_worldPos  = wpos.xyz;
    v_texcoord0 = a_texcoord0;
    vec3 nrm    = mul(u_model[0], vec4(a_normal, 0.0)).xyz;
    v_normalWS  = normalize(nrm);
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
}
