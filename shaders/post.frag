#version 450

// Post-process: tone/color grading. Sampling an sRGB offscreen decodes to linear;
// we grade here and the swapchain write re-encodes to sRGB.
layout(binding = 0) uniform sampler2D sceneColor;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

// --- Grade params (tune to taste) ---
const float EXPOSURE   = 1.03;
const float CONTRAST   = 1.05;   // around mid grey
const float SATURATION = 1.10;
const vec3  SHADOW_TINT = vec3(1.00, 0.99, 0.98); // neutral / faintly warm shadows
const vec3  HIGH_TINT   = vec3(1.07, 1.02, 0.92); // warmer highlights
const float VIGNETTE    = 0.22;  // 0 = off, 1 = strong

void main() {
    vec3 c = texture(sceneColor, uv).rgb;

    // exposure
    c *= EXPOSURE;

    // contrast around mid grey
    c = (c - 0.5) * CONTRAST + 0.5;

    // saturation
    float luma = dot(c, vec3(0.299, 0.587, 0.114));
    c = mix(vec3(luma), c, SATURATION);

    // split tone: cool shadows, warm highlights (by luminance)
    float t = clamp(dot(c, vec3(0.299, 0.587, 0.114)), 0.0, 1.0);
    c *= mix(SHADOW_TINT, HIGH_TINT, t);

    // vignette (screen-space radial)
    float vig = smoothstep(0.85, 0.25, length(uv - 0.5));
    c *= mix(1.0 - VIGNETTE, 1.0, vig);

    outColor = vec4(clamp(c, 0.0, 1.0), 1.0);
}
