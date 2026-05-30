#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

enum class TileType : uint8_t {
    AIR = 0,
    GRASS,
    DIRT,
    WATER,
    STONE,
    WOOD,
    LEAVES,
};

static constexpr int HOTBAR_SLOTS = 9;

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
};

struct InstanceData {
    glm::vec3 pos;
    glm::vec3 topColor;
    glm::vec3 sideColor;
};

// Chunk mesh vertex — color baked per-vertex (no instancing)
struct ChunkVertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec3 color;  // top face uses topColor, side/bottom use sideColor
};

// UI vertex — screen-space NDC position + RGBA color
struct UIVertex {
    glm::vec2 pos;
    glm::vec4 color;
};

// Object instance — per-tree transform (mesh reuses ChunkVertex)
struct ObjectInstance {
    glm::vec3 pos;
    float     scale;
    float     rot;   // radians around Z
};
