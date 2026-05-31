#pragma once
#include "renderer/Types.h"
#include <glm/glm.hpp>
#include <functional>
#include <vector>

static constexpr int CHUNK_SIZE  = 32;
static constexpr int CHUNK_DEPTH = 8;

struct TileState {
    uint8_t  growthStage    = 0;
    uint32_t lastUpdatedDay = 0;
    bool     watered        = false; // farmland; transient (resets daily, not saved)
};

enum class ObjectType : uint8_t {
    TREE = 0,
};

// World prop placed on top of the tile grid (rendered as a low-poly model, not a voxel)
struct Object {
    glm::vec3  pos;    // world position of the base
    float      scale;
    float      rot;    // radians around Z
    ObjectType type;
};

struct Chunk {
    TileType  tiles [CHUNK_DEPTH][CHUNK_SIZE][CHUNK_SIZE] = {};
    TileState states[CHUNK_DEPTH][CHUNK_SIZE][CHUNK_SIZE] = {};
    std::vector<Object> objects;
    bool dirty    = true;
    bool modified = false; // true if player has changed any tile (used for save/load)
};

struct IVec2Hash {
    size_t operator()(const glm::ivec2& v) const {
        size_t h1 = std::hash<int>()(v.x);
        size_t h2 = std::hash<int>()(v.y);
        return h1 ^ (h2 * 2654435761u);
    }
};
