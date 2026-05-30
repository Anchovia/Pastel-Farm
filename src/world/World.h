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

    bool inBounds(int x, int y) const;
    bool isWalkable(int x, int y) const;
    glm::ivec2 worldToTile(const glm::vec3& position) const;
    glm::vec3 tileCenter(int x, int y) const;

    static glm::vec3 tileColor(TileType type);

private:
    TileType m_grid[HEIGHT][WIDTH];
};
