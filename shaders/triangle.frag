#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 lightDir; // xyz = toward sun, w = dayFactor
} ubo;

layout(location = 0) flat in vec3 fragNormal;
layout(location = 1) flat in vec3 fragTopColor;
layout(location = 2) flat in vec3 fragSideColor;
layout(location = 0) out vec4 outColor;

void main() {
    vec3  lightDir  = normalize(ubo.lightDir.xyz);
    float dayFactor = ubo.lightDir.w;

    float diff    = max(dot(normalize(fragNormal), lightDir), 0.0);
    float ambient = mix(0.15, 0.3, dayFactor);
    float light   = ambient + diff * 0.7 * dayFactor;

    // top face (normal.z > 0.9) uses topColor, sides use sideColor
    float isTop = step(0.9, fragNormal.z);
    vec3  color = mix(fragSideColor, fragTopColor, isTop);

    outColor = vec4(color * light, 1.0);
}
