#pragma once
#include "game/Player.h"

#include <optional>

class World;

struct PlayerInput {
    bool moveForward = false;
    bool moveBackward = false;
    bool moveLeft = false;
    bool moveRight = false;
    double mouseX = 0.0;
    double mouseY = 0.0;
    bool leftClick = false;
    int windowWidth = 1280;
    int windowHeight = 720;
};

class GameState {
public:
    void update(float dt, const PlayerInput& input, float cameraAngleDegrees, const World& world);

    const Player& player() const { return m_player; }
    const std::optional<glm::ivec2>& targetTile() const { return m_targetTile; }

private:
    void updateTargetTile(const World& world);

    Player m_player;
    std::optional<glm::ivec2> m_targetTile;
};
