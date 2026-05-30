#include "World.h"

#include <cmath>
#include <cstring>

static const glm::vec3 kTileColors[] = {
    {0.0f,  0.0f,  0.0f },  // AIR   (렌더링 안 됨)
    {0.45f, 0.75f, 0.30f},  // GRASS
    {0.55f, 0.35f, 0.15f},  // DIRT
    {0.20f, 0.45f, 0.70f},  // WATER
    {0.55f, 0.55f, 0.55f},  // STONE
};

World::World() {
    memset(m_grid, 0, sizeof(m_grid)); // 전체 AIR로 초기화

    using T = TileType;
    TileType ground[HEIGHT][WIDTH] = {
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
    memcpy(m_grid[0], ground, sizeof(ground)); // Z=0 레이어에 지면 배치
}

bool World::inBounds(int x, int y, int z) const {
    return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT && z >= 0 && z < DEPTH;
}

bool World::isWalkable(int x, int y, int z) const {
    if (!inBounds(x, y, z)) return false;
    TileType t = getTile(x, y, z);
    return t != TileType::AIR && t != TileType::WATER;
}

glm::ivec3 World::worldToTile(const glm::vec3& position) const {
    const float offX = (WIDTH - 1) * 0.5f;
    const float offY = (HEIGHT - 1) * 0.5f;
    // position.z = 타일 Z + 1 (플레이어는 타일 위 1유닛에 서 있음)
    return {
        static_cast<int>(std::floor(position.x + offX + 0.5f)),
        static_cast<int>(std::floor(position.y + offY + 0.5f)),
        static_cast<int>(std::round(position.z)) - 1
    };
}

glm::vec3 World::tileCenter(int x, int y, int z) const {
    const float offX = (WIDTH - 1) * 0.5f;
    const float offY = (HEIGHT - 1) * 0.5f;
    return {x - offX, y - offY, static_cast<float>(z)};
}

glm::vec3 World::tileColor(TileType type) {
    return kTileColors[(int)type];
}
