#include "World.h"

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

glm::vec3 World::tileColor(TileType type) {
    return kTileColors[(int)type];
}
