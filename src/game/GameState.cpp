#include "game/GameState.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

void GameState::update(float dt, const PlayerInput& input, float cameraAngleDegrees) {
    const float rad = glm::radians(cameraAngleDegrees);
    const glm::vec2 forward{-glm::cos(rad), -glm::sin(rad)};
    const glm::vec2 right{-glm::sin(rad), glm::cos(rad)};

    glm::vec2 move{0.0f};
    if (input.moveForward)  move += forward;
    if (input.moveBackward) move -= forward;
    if (input.moveLeft)     move -= right;
    if (input.moveRight)    move += right;

    if (glm::length(move) > 0.0f) {
        m_player.moveBy(glm::normalize(move) * m_player.moveSpeed() * dt);
    }
}
