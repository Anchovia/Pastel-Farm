#version 450

// CPU에서 올린 버텍스 버퍼를 받음 (Types.h의 Vertex 구조체와 대응)
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragColor   = inColor;
}
