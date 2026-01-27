$input v_texcoord0

#include <bgfx_shader.sh>

uniform vec4 u_color;
SAMPLER2D(s_tex, 0);

void main() {
    vec4 texColor = texture2D(s_tex, v_texcoord0);
    gl_FragColor = u_color * texColor;
}
