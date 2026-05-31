#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 lightDir; // xyz = toward sun, w = dayFactor
} ubo;

layout(location = 0) flat in vec3 fragNormal;
layout(location = 1)      in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    vec3  lightDir  = normalize(ubo.lightDir.xyz);
    float dayFactor = ubo.lightDir.w;

    float diff    = max(dot(normalize(fragNormal), lightDir), 0.0);
    float ambient = mix(0.15, 0.3, dayFactor);
    float light   = ambient + diff * 0.7 * dayFactor;
    outColor = vec4(fragColor * light, 1.0);
}
