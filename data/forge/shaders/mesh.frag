#version 450

layout(set = 0, binding = 1) uniform texture2D uTexture;
layout(set = 0, binding = 2) uniform sampler uSampler;

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(sampler2D(uTexture, uSampler), vUV);
    outColor = texColor * vColor;
}
