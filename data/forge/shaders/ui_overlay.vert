#version 450

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_col;

layout(set = 0, binding = 0) uniform UiOverlayUniforms {
    vec4 scaleBias;
} ubo;

layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec4 frag_col;

void main() {
    vec2 pos = in_pos * ubo.scaleBias.xy + ubo.scaleBias.zw;
    gl_Position = vec4(pos, 0.0, 1.0);
    frag_uv = in_uv;
    frag_col = in_col;
}
