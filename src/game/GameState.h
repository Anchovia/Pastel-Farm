#pragma once
#include "game/Player.h"

class World;

struct PlayerInput {
    bool moveForward = false;
    bool moveBackward = false;
    bool moveLeft = false;
    bool moveRight = false;
};

class GameState {
public:
    void update(float dt, const PlayerInput& input, float cameraAngleDegrees, const World& world);

    const Player& player() const { return m_player; }

private:
    Player m_player;
};
