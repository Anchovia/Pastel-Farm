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
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
};

struct InstanceData {
    glm::vec3 pos;
    glm::vec3 topColor;
    glm::vec3 sideColor;
};

// 청크 메시 전용 정점 — 색상이 버텍스에 구워짐 (인스턴싱 없음)
struct ChunkVertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec3 color;  // 윗면이면 topColor, 옆/아랫면이면 sideColor
};
