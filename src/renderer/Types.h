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
    FARMLAND,
    WHEAT,
};

enum class ItemType : uint8_t {
    NONE = 0,
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_WOOD,
    BLOCK_LEAVES,
    BLOCK_WATER,
    TOOL_HOE,
    TOOL_AXE,
    SEED_WHEAT,
    TOOL_WATERINGCAN,
    TOOL_SICKLE,
    TOOL_PICKAXE,
    ITEM_WHEAT,
    COUNT,
};

inline bool isBlock(ItemType t) {
    return t >= ItemType::BLOCK_GRASS && t <= ItemType::BLOCK_WATER;
}
inline bool isTool(ItemType t) {
    return t == ItemType::TOOL_HOE || t == ItemType::TOOL_AXE || t == ItemType::TOOL_WATERINGCAN
        || t == ItemType::TOOL_SICKLE || t == ItemType::TOOL_PICKAXE;
}
inline TileType itemToTile(ItemType t) {
    switch (t) {
        case ItemType::BLOCK_GRASS:  return TileType::GRASS;
        case ItemType::BLOCK_DIRT:   return TileType::DIRT;
        case ItemType::BLOCK_STONE:  return TileType::STONE;
        case ItemType::BLOCK_WOOD:   return TileType::WOOD;
        case ItemType::BLOCK_LEAVES: return TileType::LEAVES;
        case ItemType::BLOCK_WATER:  return TileType::WATER;
        default:                     return TileType::AIR;
    }
}
inline glm::vec3 itemColor(ItemType t) {
    switch (t) {
        case ItemType::BLOCK_GRASS:  return {0.45f, 0.75f, 0.30f};
        case ItemType::BLOCK_DIRT:   return {0.55f, 0.35f, 0.15f};
        case ItemType::BLOCK_STONE:  return {0.55f, 0.55f, 0.55f};
        case ItemType::BLOCK_WOOD:   return {0.42f, 0.28f, 0.15f};
        case ItemType::BLOCK_LEAVES: return {0.30f, 0.55f, 0.25f};
        case ItemType::BLOCK_WATER:  return {0.20f, 0.45f, 0.70f};
        case ItemType::TOOL_HOE:     return {0.80f, 0.70f, 0.50f};
        case ItemType::TOOL_AXE:     return {0.50f, 0.50f, 0.55f};
        case ItemType::SEED_WHEAT:   return {0.80f, 0.75f, 0.20f};
        case ItemType::TOOL_WATERINGCAN: return {0.30f, 0.55f, 0.80f};
        case ItemType::TOOL_SICKLE:  return {0.70f, 0.72f, 0.45f};
        case ItemType::TOOL_PICKAXE: return {0.45f, 0.48f, 0.55f};
        case ItemType::ITEM_WHEAT:   return {0.90f, 0.75f, 0.15f};
        default:                     return {0.0f,  0.0f,  0.0f};
    }
}

// Inventory item stack (a slot holds one item type + a count)
struct ItemStack {
    ItemType type  = ItemType::NONE;
    int      count = 0;
};

// An item lying in the world: spawned on harvest, picked up on player proximity
struct DroppedItem {
    glm::vec3 pos   = {0.0f, 0.0f, 0.0f};
    ItemType  type  = ItemType::NONE;
    int       count = 0;
};

// Inventory grid layout (shared between GameState and VulkanContext)
static constexpr int   INV_COLS      = 9;   // first row == hotbar
static constexpr int   INV_ROWS      = 3;
static constexpr float INV_SLOT_SIZE = 52.0f;
static constexpr float INV_GAP       = 8.0f;
static constexpr float INV_PAD       = 16.0f;

static constexpr int HOTBAR_SLOTS = 9;
static constexpr int INV_SLOTS    = INV_COLS * INV_ROWS; // 27 (hotbar 0..8 + backpack)

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
