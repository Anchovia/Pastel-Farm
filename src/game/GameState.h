#pragma once
#include "game/Player.h"

struct PlayerInput {
    bool moveForward = false;
    bool moveBackward = false;
    bool moveLeft = false;
    bool moveRight = false;
};

class GameState {
public:
    void update(float dt, const PlayerInput& input, float cameraAngleDegrees);

    const Player& player() const { return m_player; }

private:
    Player m_player;
};
