#include "TerrainGen.h"
#include <cmath>

float TerrainGen::hash(int x, int y) {
    unsigned int h = (unsigned int)((x * 374761393) ^ (y * 668265263));
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return static_cast<float>(h) / static_cast<float>(0xFFFFFFFFu);
}

float TerrainGen::valueNoise(float x, float y) {
    int   ix = static_cast<int>(std::floor(x));
    int   iy = static_cast<int>(std::floor(y));
    float fx = x - ix;
    float fy = y - iy;
    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uy = fy * fy * (3.0f - 2.0f * fy);

    float v00 = hash(ix,     iy);
    float v10 = hash(ix + 1, iy);
    float v01 = hash(ix,     iy + 1);
    float v11 = hash(ix + 1, iy + 1);

    return (v00 * (1.0f - ux) + v10 * ux) * (1.0f - uy)
         + (v01 * (1.0f - ux) + v11 * ux) * uy;
}

float TerrainGen::fbm(float x, float y) {
    float value     = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    for (int i = 0; i < 4; i++) {
        value     += valueNoise(x * frequency, y * frequency) * amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    return value;
}

void TerrainGen::generate(int cx, int cy, Chunk& chunk) {
    constexpr float HEIGHT_SCALE = 1.0f / 32.0f;
    constexpr float BIOME_SCALE  = 1.0f / 24.0f;

    for (int ly = 0; ly < CHUNK_SIZE; ly++) {
        for (int lx = 0; lx < CHUNK_SIZE; lx++) {
            const int wx = cx * CHUNK_SIZE + lx;
            const int wy = cy * CHUNK_SIZE + ly;

            const float h = fbm(wx * HEIGHT_SCALE, wy * HEIGHT_SCALE);
            const float b = fbm(wx * BIOME_SCALE + 100.0f, wy * BIOME_SCALE + 100.0f);

            // Z=0: always solid ground
            chunk.tiles[0][ly][lx] = (b < 0.35f) ? TileType::DIRT : TileType::GRASS;

            // Z=1: hill
            if (h > 0.45f)
                chunk.tiles[1][ly][lx] = (b < 0.3f) ? TileType::DIRT : TileType::GRASS;

            // Z=2: peak — stone
            if (h > 0.65f)
                chunk.tiles[2][ly][lx] = TileType::STONE;
        }
    }
    chunk.dirty = true;
}
