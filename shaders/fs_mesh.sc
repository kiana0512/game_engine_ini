$input v_normal
#include "bgfx_shader.sh"
uniform vec4 u_lightDir;
void main(){vec3 n=normalize(v_normal);vec3 ld=normalize(u_lightDir.xyz);float d=max(dot(n,ld),0.0);gl_FragColor=vec4(d,d,d,1.0);}
