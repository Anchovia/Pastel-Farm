#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 lightDir;
    mat4 lightMVP;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 instancePos;
layout(location = 3) in vec3 instanceTopColor;
layout(location = 4) in vec3 instanceSideColor;

layout(location = 0) flat out vec3 fragNormal;
layout(location = 1) flat out vec3 fragTopColor;
layout(location = 2) flat out vec3 fragSideColor;
layout(location = 3)      out vec4 fragPosLightSpace;

void main() {
    vec3 worldPos     = inPosition + instancePos;
    gl_Position       = ubo.proj * ubo.view * vec4(worldPos, 1.0);
    fragNormal        = inNormal;
    fragTopColor      = instanceTopColor;
    fragSideColor     = instanceSideColor;
    fragPosLightSpace = ubo.lightMVP * vec4(worldPos, 1.0);
}
