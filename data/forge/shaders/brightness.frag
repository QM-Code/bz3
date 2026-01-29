#version 450

layout(location = 0) in vec2 frag_uv;

layout(set = 0, binding = 0) uniform BrightnessUniforms {
    vec4 scaleBias;
    float brightness;
    float pad0;
    float pad1;
    float pad2;
} ubo;

layout(set = 0, binding = 1) uniform texture2D uTexture;
layout(set = 0, binding = 2) uniform sampler uSampler;

layout(location = 0) out vec4 out_color;

void main() {
    vec4 tex = texture(sampler2D(uTexture, uSampler), frag_uv);
    out_color = tex * ubo.brightness;
}
