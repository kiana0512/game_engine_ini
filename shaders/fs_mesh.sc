$input v_normal, v_texcoord0

#include <bgfx_shader.sh>

uniform vec4 u_lightDir;
SAMPLER2D(s_texColor, 0);

void main()
{
    vec3 N = normalize(v_normal);
    vec3 L = normalize(u_lightDir.xyz);
    float NdotL = max(dot(N, L), 0.0);
    vec3 albedo = texture2D(s_texColor, v_texcoord0).rgb;
    float ambient = clamp(u_lightDir.w, 0.0, 1.0);
    vec3 color = albedo * (ambient + NdotL);
    gl_FragColor = vec4(color, 1.0);
}
