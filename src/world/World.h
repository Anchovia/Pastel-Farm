#pragma once
#include "renderer/Types.h"
#include <glm/glm.hpp>

class World {
public:
    static constexpr int WIDTH  = 10;
    static constexpr int HEIGHT = 10;

    World();

    TileType  getTile(int x, int y) const       { return m_grid[y][x]; }
    void      setTile(int x, int y, TileType t) { m_grid[y][x] = t; }

    static glm::vec3 tileColor(TileType type);

private:
    TileType m_grid[HEIGHT][WIDTH];
};
