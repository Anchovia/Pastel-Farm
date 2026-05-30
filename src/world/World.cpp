#include "World.h"
#include <cmath>

static const glm::vec3 kTileColors[] = {
    {0.0f,  0.0f,  0.0f },  // AIR   (렌더링 안 됨)
    {0.45f, 0.75f, 0.30f},  // GRASS
    {0.55f, 0.35f, 0.15f},  // DIRT
    {0.20f, 0.45f, 0.70f},  // WATER
    {0.55f, 0.55f, 0.55f},  // STONE
};

World::World() {
    using T = TileType;
    const TileType ground[10][10] = {
        { T::WATER, T::WATER, T::WATER, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS },
        { T::WATER, T::WATER, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS },
        { T::WATER, T::GRASS, T::GRASS, T::DIRT,  T::DIRT,  T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS },
        { T::GRASS, T::GRASS, T::GRASS, T::DIRT,  T::DIRT,  T::GRASS, T::GRASS, T::STONE, T::STONE, T::GRASS },
        { T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::STONE, T::STONE, T::GRASS },
        { T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS },
        { T::GRASS, T::GRASS, T::DIRT,  T::DIRT,  T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS },
        { T::GRASS, T::GRASS, T::DIRT,  T::DIRT,  T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS },
        { T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS },
        { T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS, T::GRASS },
    };

    Chunk& chunk = getOrCreateChunk(0, 0);
    for (int y = 0; y < 10; y++)
        for (int x = 0; x < 10; x++)
            chunk.tiles[0][y][x] = ground[y][x];
}

glm::ivec2 World::chunkCoord(int x, int y) {
    return {
        (int)std::floor((float)x / CHUNK_SIZE),
        (int)std::floor((float)y / CHUNK_SIZE)
    };
}

glm::ivec2 World::localCoord(int x, int y) {
    return {
        ((x % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE,
        ((y % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE
    };
}

Chunk& World::getOrCreateChunk(int cx, int cy) {
    return m_chunks[{cx, cy}];
}

const Chunk* World::getChunk(int cx, int cy) const {
    auto it = m_chunks.find({cx, cy});
    return it != m_chunks.end() ? &it->second : nullptr;
}

TileType World::getTile(int x, int y, int z) const {
    if (z < 0 || z >= CHUNK_DEPTH) return TileType::AIR;
    auto cc = chunkCoord(x, y);
    const Chunk* chunk = getChunk(cc.x, cc.y);
    if (!chunk) return TileType::AIR;
    auto lc = localCoord(x, y);
    return chunk->tiles[z][lc.y][lc.x];
}

void World::setTile(int x, int y, int z, TileType t) {
    if (z < 0 || z >= CHUNK_DEPTH) return;
    auto cc = chunkCoord(x, y);
    Chunk& chunk = getOrCreateChunk(cc.x, cc.y);
    auto lc = localCoord(x, y);
    chunk.tiles[z][lc.y][lc.x] = t;
    chunk.dirty = true;
}

bool World::inBounds(int x, int y, int z) const {
    return z >= 0 && z < CHUNK_DEPTH;
}

bool World::isWalkable(int x, int y, int z) const {
    TileType t = getTile(x, y, z);
    return t != TileType::AIR && t != TileType::WATER;
}

glm::ivec3 World::worldToTile(const glm::vec3& position) const {
    return {
        static_cast<int>(std::floor(position.x + 0.5f)),
        static_cast<int>(std::floor(position.y + 0.5f)),
        static_cast<int>(std::round(position.z)) - 1
    };
}

glm::vec3 World::tileCenter(int x, int y, int z) const {
    return { static_cast<float>(x), static_cast<float>(y), static_cast<float>(z) };
}

glm::vec3 World::tileColor(TileType type) {
    return kTileColors[(int)type];
}
