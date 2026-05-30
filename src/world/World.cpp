#include "World.h"

#include <cmath>
#include <cstring>

static const glm::vec3 kTileColors[] = {
    {0.45f, 0.75f, 0.30f},  // GRASS
    {0.55f, 0.35f, 0.15f},  // DIRT
    {0.20f, 0.45f, 0.70f},  // WATER
    {0.55f, 0.55f, 0.55f},  // STONE
};

World::World() {
    using T = TileType;
    TileType init[HEIGHT][WIDTH] = {
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
    memcpy(m_grid, init, sizeof(m_grid));
}

bool World::inBounds(int x, int y) const {
    return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT;
}

bool World::isWalkable(int x, int y) const {
    return inBounds(x, y) && getTile(x, y) != TileType::WATER;
}

glm::ivec2 World::worldToTile(const glm::vec3& position) const {
    const float offX = (WIDTH - 1) * 0.5f;
    const float offY = (HEIGHT - 1) * 0.5f;
    return {
        static_cast<int>(std::floor(position.x + offX + 0.5f)),
        static_cast<int>(std::floor(position.y + offY + 0.5f))
    };
}

glm::vec3 World::tileCenter(int x, int y) const {
    const float offX = (WIDTH - 1) * 0.5f;
    const float offY = (HEIGHT - 1) * 0.5f;
    return {x - offX, y - offY, 0.0f};
}

glm::vec3 World::tileColor(TileType type) {
    return kTileColors[(int)type];
}
