#version 450

layout(location = 0) flat in vec3 fragNormal;
layout(location = 1) flat in vec3 fragTopColor;
layout(location = 2) flat in vec3 fragSideColor;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightDir = normalize(vec3(1.0, 1.0, 2.0));
    float diff    = max(dot(normalize(fragNormal), lightDir), 0.0);
    float light   = 0.3 + diff * 0.7;

    // 윗면(normal.z > 0.9)은 topColor, 나머지는 sideColor
    float isTop = step(0.9, fragNormal.z);
    vec3  color = mix(fragSideColor, fragTopColor, isTop);

    outColor = vec4(color * light, 1.0);
}
