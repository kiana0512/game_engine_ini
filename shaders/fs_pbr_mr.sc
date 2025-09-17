$input v_texcoord0, v_worldPos, v_normalWS
#include "bgfx_shader.sh"

uniform vec4 u_lightDir;
uniform vec4 u_pointPosRad;
uniform vec4 u_pointColInt;
uniform vec4 u_viewPosExp;

uniform vec4 u_baseColorFactor;
uniform vec4 u_mrFactor;
uniform vec4 u_emissive;
uniform vec4 u_matFlags;

SAMPLER2D(s_baseColor, 0);
SAMPLER2D(s_mr,        1);
SAMPLER2D(s_normal,    2);
SAMPLER2D(s_ao,        3);
SAMPLER2D(s_emissive,  4);

vec3 srgbToLinear(vec3 c){ return pow(c, vec3_splat(2.2)); }
vec3 F_Schlick(vec3 F0, float ct){ return F0 + (1.0 - F0) * pow(1.0 - ct, 5.0); }
float D_GGX(float NoH, float a){ float a2=a*a; float d=(NoH*NoH)*(a2-1.0)+1.0; return a2/(3.14159265*d*d); }
float V_SmithGGXCorrelated(float NoV,float NoL,float a){ float a2=a*a; float gv=NoL*sqrt((NoV-NoV*a2)*NoV+a2); float gl=NoV*sqrt((NoL-NoL*a2)*NoL+a2); return 0.5/(gv+gl); }
vec3 tonemapACES(vec3 x){ const float A=2.51,B=0.03,C=2.43,D=0.59,E=0.14; return clamp((x*(A*x+B))/(x*(C*x+D)+E), 0.0, 1.0); }

void main()
{
    vec4 baseTex = texture2D(s_baseColor, v_texcoord0);
    vec3 baseCol = srgbToLinear(baseTex.rgb) * u_baseColorFactor.rgb;

    float metallic  = u_mrFactor.x;
    float roughness = u_mrFactor.y;
    vec2 mrTex = texture2D(s_mr, v_texcoord0).bg;
    if ( (int(u_matFlags.x) & 2) != 0 ) { metallic = mrTex.y; roughness = mrTex.x; }
    roughness = clamp(roughness, 0.045, 1.0);

    vec3 N = normalize(v_normalWS);
    vec3 V = normalize(u_viewPosExp.xyz - v_worldPos);
    float NoV = saturate(dot(N,V));
    float a = roughness*roughness;
    vec3 F0 = mix(vec3_splat(0.04), baseCol, metallic);

    vec3 Ld = normalize(u_lightDir.xyz);
    float NoLd = saturate(dot(N,Ld));
    vec3 H = normalize(V+Ld);
    float NoH = saturate(dot(N,H));
    vec3 F = F_Schlick(F0, saturate(dot(H,V)));
    float D = D_GGX(NoH,a);
    float Vis = V_SmithGGXCorrelated(NoV,NoLd,a);
    vec3 spec_d = F*D*Vis;
    vec3 diff_d = (1.0 - F) * (1.0 - metallic) * baseCol / 3.14159265;
    vec3 Lo_dir = (diff_d + spec_d) * NoLd;

    vec3 Lp = normalize(u_pointPosRad.xyz - v_worldPos);
    float NoLp = saturate(dot(N,Lp));
    float dist = length(u_pointPosRad.xyz - v_worldPos);
    float radius = u_pointPosRad.w;
    float att = saturate(1.0 - dist / max(radius, 0.0001));
    att = att*att*(3.0 - 2.0*att);
    vec3 H2 = normalize(V+Lp);
    float NoH2 = saturate(dot(N,H2));
    vec3 F2 = F_Schlick(F0, saturate(dot(H2,V)));
    float D2 = D_GGX(NoH2,a);
    float Vis2 = V_SmithGGXCorrelated(NoV,NoLp,a);
    vec3 spec_p = F2*D2*Vis2;
    vec3 diff_p = (1.0 - F2) * (1.0 - metallic) * baseCol / 3.14159265;
    vec3 Lo_point = (diff_p + spec_p) * u_pointColInt.rgb * u_pointColInt.a * NoLp * att;

    vec3 ambient = baseCol * u_lightDir.w * (1.0 - metallic);

    vec3 color = ambient + Lo_dir + Lo_point + u_emissive.rgb;
    color *= u_viewPosExp.w;
    color = tonemapACES(color);
    color = pow(color, vec3_splat(1.0/2.2));
    gl_FragColor = vec4(color,1.0);
}
