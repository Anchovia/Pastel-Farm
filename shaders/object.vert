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
layout(location = 3) in vec3  instancePos;
layout(location = 4) in float instanceScale;
layout(location = 5) in float instanceRot;

layout(location = 0) flat out vec3 fragNormal;
layout(location = 1)      out vec3 fragColor;
layout(location = 2)      out vec4 fragPosLightSpace;

void main() {
    float s = sin(instanceRot);
    float c = cos(instanceRot);
    // Rotate around Z
    vec3 p  = inPosition * instanceScale;
    vec3 rp = vec3(p.x * c - p.y * s, p.x * s + p.y * c, p.z);
    vec3 worldPos = rp + instancePos;

    gl_Position       = ubo.proj * ubo.view * vec4(worldPos, 1.0);
    fragNormal        = vec3(inNormal.x * c - inNormal.y * s, inNormal.x * s + inNormal.y * c, inNormal.z);
    fragColor         = inColor;
    fragPosLightSpace = ubo.lightMVP * vec4(worldPos, 1.0);
}
