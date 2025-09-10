$input v_normal, v_texcoord0, v_worldPos

#include <bgfx_shader.sh>

uniform vec4 u_lightDir;
uniform vec4 u_pointPosRad;
uniform vec4 u_pointColInt;
SAMPLER2D(s_texColor, 0);

void main()
{
    vec3 N = normalize(v_normal);
    vec3 albedo = texture2D(s_texColor, v_texcoord0).rgb;
    vec3 Ld = normalize(u_lightDir.xyz);
    float ndl = max(dot(N, Ld), 0.0);
    float ambient = clamp(u_lightDir.w, 0.0, 1.0);
    vec3 color = albedo * (ambient + ndl);
    vec3 toP = u_pointPosRad.xyz - v_worldPos;
    float dist = length(toP);
    float R = max(u_pointPosRad.w, 1e-5);
    float x = clamp(dist / R, 0.0, 1.0);
    float att = (1.0 - x);
    att = att * att * (3.0 - 2.0 * att);
    vec3 Lp = (dist > 1e-5) ? (toP / dist) : vec3(0.0, 1.0, 0.0);
    float ndlp = max(dot(N, Lp), 0.0);
    vec3 pointColor = albedo * ndlp * u_pointColInt.xyz * u_pointColInt.w * att;
    color += pointColor;
    gl_FragColor = vec4(color, 1.0);
}
