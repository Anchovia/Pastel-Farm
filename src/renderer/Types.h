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
