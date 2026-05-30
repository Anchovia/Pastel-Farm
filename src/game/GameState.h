#pragma once
#include "game/Player.h"

#include <optional>

class World;
class Camera;

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
    void update(float dt, const PlayerInput& input, const Camera& camera, const World& world);

    const Player& player() const { return m_player; }
    const std::optional<glm::ivec3>& targetTile() const { return m_targetTile; }

private:
    void updateTargetTile(const World& world);

    Player m_player;
    std::optional<glm::ivec3> m_targetTile;
};
