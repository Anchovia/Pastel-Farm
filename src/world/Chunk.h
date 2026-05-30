#pragma once
#include "renderer/Types.h"
#include <glm/glm.hpp>
#include <functional>

static constexpr int CHUNK_SIZE  = 16;
static constexpr int CHUNK_DEPTH = 8;

struct TileState {
    uint8_t  growthStage    = 0;
    uint32_t lastUpdatedDay = 0;
};

struct Chunk {
    TileType  tiles [CHUNK_DEPTH][CHUNK_SIZE][CHUNK_SIZE] = {};
    TileState states[CHUNK_DEPTH][CHUNK_SIZE][CHUNK_SIZE] = {};
    bool      dirty = true;
};

struct IVec2Hash {
    size_t operator()(const glm::ivec2& v) const {
        size_t h1 = std::hash<int>()(v.x);
        size_t h2 = std::hash<int>()(v.y);
        return h1 ^ (h2 * 2654435761u);
    }
};
