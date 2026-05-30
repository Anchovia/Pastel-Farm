#include "game/GameState.h"
#include "world/World.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace {
bool canOccupy(const World& world, const glm::vec3& position) {
    const glm::ivec2 tile = world.worldToTile(position);
    return world.isWalkable(tile.x, tile.y);
}
}

void GameState::update(float dt, const PlayerInput& input, float cameraAngleDegrees, const World& world) {
    const float rad = glm::radians(cameraAngleDegrees);
    const glm::vec2 forward{-glm::cos(rad), -glm::sin(rad)};
    const glm::vec2 right{-glm::sin(rad), glm::cos(rad)};

    glm::vec2 move{0.0f};
    if (input.moveForward)  move += forward;
    if (input.moveBackward) move -= forward;
    if (input.moveLeft)     move -= right;
    if (input.moveRight)    move += right;

    if (glm::length(move) > 0.0f) {
        const glm::vec2 delta = glm::normalize(move) * m_player.moveSpeed() * dt;

        glm::vec3 next = m_player.position();
        next.x += delta.x;
        if (canOccupy(world, next)) {
            m_player.moveBy({delta.x, 0.0f});
        }

        next = m_player.position();
        next.y += delta.y;
        if (canOccupy(world, next)) {
            m_player.moveBy({0.0f, delta.y});
        }
    }
}
