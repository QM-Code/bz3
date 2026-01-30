#version 450

layout(location = 0) in vec3 inPos;

layout(set = 0, binding = 0) uniform MeshConstants {
    mat4 uMVP;
    vec4 uColor;
} constants;

layout(location = 0) out vec4 vColor;

void main() {
    gl_Position = constants.uMVP * vec4(inPos, 1.0);
    vColor = constants.uColor;
}
