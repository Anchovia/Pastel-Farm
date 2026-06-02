#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 lightDir;
    mat4 lightMVP;
    vec4 fogColor;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 instancePos;
layout(location = 4) in float instanceScale;
layout(location = 5) in float instanceRot;

layout(location = 0)      out vec3 fragNormal;
layout(location = 1)      out vec2 fragUV;
layout(location = 2)      out vec4 fragPosLightSpace;
layout(location = 3)      out float fragViewDepth;

void main() {
    float s = sin(instanceRot);
    float c = cos(instanceRot);

    vec3 p  = inPosition * instanceScale;
    vec3 rp = vec3(p.x * c - p.y * s, p.x * s + p.y * c, p.z);
    vec3 worldPos = rp + instancePos;

    vec3 n = inNormal;
    vec3 rn = vec3(n.x * c - n.y * s, n.x * s + n.y * c, n.z);

    vec4 viewPos      = ubo.view * vec4(worldPos, 1.0);
    gl_Position       = ubo.proj * viewPos;
    fragNormal        = rn;
    fragUV            = inUV;
    fragPosLightSpace = ubo.lightMVP * vec4(worldPos, 1.0);
    fragViewDepth     = -viewPos.z;
}
