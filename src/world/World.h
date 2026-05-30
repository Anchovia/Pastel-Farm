#pragma once
#include "world/Chunk.h"
#include <glm/glm.hpp>
#include <unordered_map>

class World {
public:
    World();

    TileType getTile(int x, int y, int z) const;
    void     setTile(int x, int y, int z, TileType t);

    bool       inBounds(int x, int y, int z) const;
    bool       isWalkable(int x, int y, int z) const;
    glm::ivec3 worldToTile(const glm::vec3& position) const;
    glm::vec3  tileCenter(int x, int y, int z) const;

    static glm::vec3 tileColor(TileType type);

    const std::unordered_map<glm::ivec2, Chunk, IVec2Hash>& chunks() const { return m_chunks; }
          std::unordered_map<glm::ivec2, Chunk, IVec2Hash>& chunks()       { return m_chunks; }

private:
    Chunk&       getOrCreateChunk(int cx, int cy);
    const Chunk* getChunk(int cx, int cy) const;

    static glm::ivec2 chunkCoord(int x, int y);
    static glm::ivec2 localCoord(int x, int y);

    std::unordered_map<glm::ivec2, Chunk, IVec2Hash> m_chunks;
};
