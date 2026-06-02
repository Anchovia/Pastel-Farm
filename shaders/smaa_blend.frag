#version 450

// SMAA 1x pass 2: horizontal/vertical blending weight calculation.
layout(binding = 0) uniform sampler2D edgesTex;
layout(binding = 1) uniform sampler2D areaTex;
layout(binding = 2) uniform sampler2D searchTex;

layout(push_constant) uniform PostPushConstants {
    vec4 params; // xy = inverse framebuffer size, z = AA mode, w = unused
} pc;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

const int   SMAA_MAX_SEARCH_STEPS = 16;
const float SMAA_AREATEX_MAX_DISTANCE = 16.0;
const vec2  SMAA_AREATEX_PIXEL_SIZE = vec2(1.0 / 160.0, 1.0 / 560.0);
const float SMAA_AREATEX_SUBTEX_SIZE = 1.0 / 7.0;
const vec2  SMAA_SEARCHTEX_SIZE = vec2(66.0, 33.0);
const vec2  SMAA_SEARCHTEX_PACKED_SIZE = vec2(64.0, 16.0);
const float SMAA_CORNER_ROUNDING_NORM = 0.25;

vec2 sampleEdges(vec2 p) {
    return textureLod(edgesTex, clamp(p, vec2(0.0), vec2(1.0)), 0.0).rg;
}

vec2 sampleEdgesOffset(vec2 p, ivec2 offset) {
    vec2 coord = p + vec2(offset) * pc.params.xy;
    return textureLod(edgesTex, clamp(coord, vec2(0.0), vec2(1.0)), 0.0).rg;
}

float searchLength(vec2 e, float offset) {
    vec2 scale = SMAA_SEARCHTEX_SIZE * vec2(0.5, -1.0);
    vec2 bias = SMAA_SEARCHTEX_SIZE * vec2(offset, 1.0);

    scale += vec2(-1.0, 1.0);
    bias += vec2(0.5, -0.5);

    scale /= SMAA_SEARCHTEX_PACKED_SIZE;
    bias /= SMAA_SEARCHTEX_PACKED_SIZE;

    return textureLod(searchTex, scale * e + bias, 0.0).r;
}

float searchXLeft(vec2 texcoord, float end) {
    vec2 r = pc.params.xy;
    vec2 e = vec2(0.0, 1.0);
    while (texcoord.x > end && e.g > 0.8281 && e.r == 0.0) {
        e = sampleEdges(texcoord);
        texcoord -= vec2(2.0 * r.x, 0.0);
    }
    float offset = -(255.0 / 127.0) * searchLength(e, 0.0) + 3.25;
    return texcoord.x + r.x * offset;
}

float searchXRight(vec2 texcoord, float end) {
    vec2 r = pc.params.xy;
    vec2 e = vec2(0.0, 1.0);
    while (texcoord.x < end && e.g > 0.8281 && e.r == 0.0) {
        e = sampleEdges(texcoord);
        texcoord += vec2(2.0 * r.x, 0.0);
    }
    float offset = -(255.0 / 127.0) * searchLength(e, 0.5) + 3.25;
    return texcoord.x - r.x * offset;
}

float searchYUp(vec2 texcoord, float end) {
    vec2 r = pc.params.xy;
    vec2 e = vec2(1.0, 0.0);
    while (texcoord.y > end && e.r > 0.8281 && e.g == 0.0) {
        e = sampleEdges(texcoord);
        texcoord -= vec2(0.0, 2.0 * r.y);
    }
    float offset = -(255.0 / 127.0) * searchLength(e.gr, 0.0) + 3.25;
    return texcoord.y + r.y * offset;
}

float searchYDown(vec2 texcoord, float end) {
    vec2 r = pc.params.xy;
    vec2 e = vec2(1.0, 0.0);
    while (texcoord.y < end && e.r > 0.8281 && e.g == 0.0) {
        e = sampleEdges(texcoord);
        texcoord += vec2(0.0, 2.0 * r.y);
    }
    float offset = -(255.0 / 127.0) * searchLength(e.gr, 0.5) + 3.25;
    return texcoord.y - r.y * offset;
}

vec2 area(vec2 dist, float e1, float e2, float offset) {
    vec2 texcoord = SMAA_AREATEX_MAX_DISTANCE * round(4.0 * vec2(e1, e2)) + dist;
    texcoord = SMAA_AREATEX_PIXEL_SIZE * texcoord + 0.5 * SMAA_AREATEX_PIXEL_SIZE;
    texcoord.y = SMAA_AREATEX_SUBTEX_SIZE * offset + texcoord.y;
    return textureLod(areaTex, texcoord, 0.0).rg;
}

void detectHorizontalCornerPattern(inout vec2 weights, vec4 coords, vec2 d) {
    vec2 leftRight = step(d.xy, d.yx);
    vec2 rounding = (1.0 - SMAA_CORNER_ROUNDING_NORM) * leftRight;
    rounding /= max(leftRight.x + leftRight.y, 0.0001);

    vec2 factor = vec2(1.0);
    factor.x -= rounding.x * sampleEdgesOffset(coords.xy, ivec2(0, 1)).r;
    factor.x -= rounding.y * sampleEdgesOffset(coords.zw, ivec2(1, 1)).r;
    factor.y -= rounding.x * sampleEdgesOffset(coords.xy, ivec2(0, -2)).r;
    factor.y -= rounding.y * sampleEdgesOffset(coords.zw, ivec2(1, -2)).r;

    weights *= clamp(factor, vec2(0.0), vec2(1.0));
}

void detectVerticalCornerPattern(inout vec2 weights, vec4 coords, vec2 d) {
    vec2 leftRight = step(d.xy, d.yx);
    vec2 rounding = (1.0 - SMAA_CORNER_ROUNDING_NORM) * leftRight;
    rounding /= max(leftRight.x + leftRight.y, 0.0001);

    vec2 factor = vec2(1.0);
    factor.x -= rounding.x * sampleEdgesOffset(coords.xy, ivec2(1, 0)).g;
    factor.x -= rounding.y * sampleEdgesOffset(coords.zw, ivec2(1, 1)).g;
    factor.y -= rounding.x * sampleEdgesOffset(coords.xy, ivec2(-2, 0)).g;
    factor.y -= rounding.y * sampleEdgesOffset(coords.zw, ivec2(-2, 1)).g;

    weights *= clamp(factor, vec2(0.0), vec2(1.0));
}

void main() {
    vec2 r = pc.params.xy;
    vec2 frameSize = 1.0 / r;
    vec2 pixcoord = uv * frameSize;

    vec4 offset0 = r.xyxy * vec4(-0.25, -0.125, 1.25, -0.125) + uv.xyxy;
    vec4 offset1 = r.xyxy * vec4(-0.125, -0.25, -0.125, 1.25) + uv.xyxy;
    vec4 offset2 = vec4(offset0.xz, offset1.yw) +
                   vec4(-2.0, 2.0, -2.0, 2.0) * r.xxyy * float(SMAA_MAX_SEARCH_STEPS);

    vec4 weights = vec4(0.0);
    vec2 e = texture(edgesTex, uv).rg;

    if (e.g > 0.0) {
        vec2 d;
        vec3 coords;
        coords.x = searchXLeft(offset0.xy, offset2.x);
        coords.y = offset1.y;
        d.x = coords.x;

        float e1 = textureLod(edgesTex, coords.xy, 0.0).r;

        coords.z = searchXRight(offset0.zw, offset2.y);
        d.y = coords.z;

        d = abs(round(frameSize.xx * d - pixcoord.xx));
        vec2 sqrtD = sqrt(d);
        float e2 = sampleEdgesOffset(coords.zy, ivec2(1, 0)).r;

        weights.rg = area(sqrtD, e1, e2, 0.0);
        coords.y = uv.y;
        detectHorizontalCornerPattern(weights.rg, coords.xyzy, d);
    }

    if (e.r > 0.0) {
        vec2 d;
        vec3 coords;
        coords.y = searchYUp(offset1.xy, offset2.z);
        coords.x = offset0.x;
        d.x = coords.y;

        float e1 = textureLod(edgesTex, coords.xy, 0.0).g;

        coords.z = searchYDown(offset1.zw, offset2.w);
        d.y = coords.z;

        d = abs(round(frameSize.yy * d - pixcoord.yy));
        vec2 sqrtD = sqrt(d);
        float e2 = sampleEdgesOffset(coords.xz, ivec2(0, 1)).g;

        weights.ba = area(sqrtD, e1, e2, 0.0);
        coords.x = uv.x;
        detectVerticalCornerPattern(weights.ba, coords.xyxz, d);
    }

    outColor = weights;
}
