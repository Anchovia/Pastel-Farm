#version 450

// Ground contact AO: a flat quad placed at each grass clump that darkens the
// terrain beneath it (decal-style soft shadow). Reuses the grass instance buffer.
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3  inPosition;   // unit quad corner (xy in [-0.5, 0.5], z = 0)
layout(location = 2) in vec2  inUV;
layout(location = 3) in vec3  instancePos;  // grass clump base position
layout(location = 4) in float instanceScale;
// location 1 (normal) and 5 (rot) are unused — a circular patch is rotation-invariant.

layout(location = 0) out vec2  fragUV;
layout(location = 1) out float fragViewDepth;

void main() {
    const float CONTACT_SIZE = 0.6; // patch diameter relative to clump scale
    vec3 worldPos = instancePos + vec3(inPosition.xy * (instanceScale * CONTACT_SIZE), 0.02);
    vec4 viewPos  = ubo.view * vec4(worldPos, 1.0);
    gl_Position   = ubo.proj * viewPos;
    fragUV        = inUV;
    fragViewDepth = -viewPos.z;
}
