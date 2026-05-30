#pragma once
#include "game/Player.h"
#include "renderer/Types.h"

#include <optional>
#include <array>

class World;
class Camera;

struct PlayerInput {
    bool moveForward = false;
    bool moveBackward = false;
    bool moveLeft = false;
    bool moveRight = false;
    double mouseX = 0.0;
    double mouseY = 0.0;
    bool leftClick  = false;
    bool rightClick = false;
    int  selectSlot  = -1;  // 0..HOTBAR_SLOTS-1 if a number key was pressed, else -1
    int  scrollDelta = 0;   // slots to move from scroll wheel
    int windowWidth = 1280;
    int windowHeight = 720;
};

class GameState {
public:
    GameState();

    void update(float dt, const PlayerInput& input, const Camera& camera, World& world);

    const Player& player() const { return m_player; }
    const std::optional<glm::ivec3>& targetTile() const { return m_targetTile; }

    int selectedSlot() const { return m_selectedSlot; }
    const std::array<TileType, HOTBAR_SLOTS>& palette() const { return m_palette; }

private:
    void updateTargetTile(World& world);

    Player m_player;
    std::optional<glm::ivec3> m_targetTile;

    int m_selectedSlot = 0;
    std::array<TileType, HOTBAR_SLOTS> m_palette;
};
