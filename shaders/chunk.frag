#version 450

layout(location = 0) flat in vec3 fragNormal;
layout(location = 1)      in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    vec3  lightDir = normalize(vec3(1.0, 1.0, 2.0));
    float diff     = max(dot(normalize(fragNormal), lightDir), 0.0);
    float light    = 0.3 + diff * 0.7;
    outColor = vec4(fragColor * light, 1.0);
}
