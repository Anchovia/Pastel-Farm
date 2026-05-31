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
layout(location = 2) in vec3 inColor;

layout(location = 0) flat out vec3 fragNormal;
layout(location = 1)      out vec3 fragColor;
layout(location = 2)      out vec4 fragPosLightSpace;

void main() {
    gl_Position       = ubo.proj * ubo.view * vec4(inPosition, 1.0);
    fragNormal        = inNormal;
    fragColor         = inColor;
    fragPosLightSpace = ubo.lightMVP * vec4(inPosition, 1.0);
}
