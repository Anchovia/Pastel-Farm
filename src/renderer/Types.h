#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

enum class TileType : uint8_t {
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
    glm::vec3 color;
};
