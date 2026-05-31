#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 lightDir;
    mat4 lightMVP;
} ubo;

layout(binding = 1) uniform sampler2DShadow shadowMap;

layout(location = 0) flat in vec3 fragNormal;
layout(location = 1)      in vec3 fragColor;
layout(location = 2)      in vec4 fragPosLightSpace;
layout(location = 0) out vec4 outColor;

void main() {
    vec3  lightDir  = normalize(ubo.lightDir.xyz);
    float dayFactor = ubo.lightDir.w;

    // Shadow map lookup
    vec3  projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords.xy    = projCoords.xy * 0.5 + 0.5;
    float shadow     = 1.0;
    if (dayFactor > 0.01 && projCoords.z >= 0.0 && projCoords.z <= 1.0) {
        float NdotL = max(dot(normalize(fragNormal), lightDir), 0.0);
        float bias  = mix(0.008, 0.001, NdotL); // more bias for steep angles to prevent acne
        shadow = texture(shadowMap, vec3(projCoords.xy, projCoords.z - bias));
    }
    float shadowFactor = max(shadow, 0.4); // shadowed areas keep 40% brightness

    float diff    = max(dot(normalize(fragNormal), lightDir), 0.0);
    float ambient = mix(0.15, 0.3, dayFactor);
    float light   = ambient + diff * 0.7 * dayFactor * shadowFactor;
    outColor = vec4(fragColor * light, 1.0);
}
