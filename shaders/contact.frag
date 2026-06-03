#version 450

layout(location = 0) in vec2  fragUV;
layout(location = 1) in float fragViewDepth;
layout(location = 0) out vec4 outColor;

void main() {
    // Soft radial falloff with no hard core: darkest at the exact center, smoothly
    // fading to nothing at the edge — reads as gentle grounding, not a visible disc.
    float d = clamp(length(fragUV * 2.0 - 1.0), 0.0, 1.0);
    float a = 1.0 - smoothstep(0.0, 1.0, d);
    // Fade out with distance so far patches don't darken fogged ground.
    a *= 1.0 - smoothstep(40.0, 57.0, fragViewDepth);
    a *= 0.12; // contact strength (subtle)
    // Black, alpha-blended → multiplies the ground darker (dst * (1 - a)).
    outColor = vec4(0.0, 0.0, 0.0, a);
}
