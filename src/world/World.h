#pragma once
#include "renderer/Types.h"
#include <glm/glm.hpp>

class World {
public:
    static constexpr int WIDTH  = 10;
    static constexpr int HEIGHT = 10;
    static constexpr int DEPTH  = 8;

    World();

    TileType  getTile(int x, int y, int z) const       { return m_grid[z][y][x]; }
    void      setTile(int x, int y, int z, TileType t) { m_grid[z][y][x] = t; }

    bool inBounds(int x, int y, int z) const;
    bool isWalkable(int x, int y, int z) const;
    glm::ivec3 worldToTile(const glm::vec3& position) const;
    glm::vec3 tileCenter(int x, int y, int z) const;

    static glm::vec3 tileColor(TileType type);

private:
    TileType m_grid[DEPTH][HEIGHT][WIDTH];
};
