#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec3 instancePos;

layout(location = 0) flat out vec3 fragNormal;
layout(location = 1) flat out vec3 fragColor;

void main() {
    vec3 worldPos  = inPosition + instancePos;
    gl_Position    = ubo.proj * ubo.view * vec4(worldPos, 1.0);
    fragNormal     = inNormal;
    fragColor      = inColor;
}
