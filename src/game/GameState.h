#pragma once
#include "game/Player.h"
#include "renderer/Types.h"

#include <optional>
#include <array>

class World;
class Camera;

static constexpr float DAY_DURATION = 120.0f; // seconds per in-game day

struct PlayerInput {
    bool moveForward = false;
    bool moveBackward = false;
    bool moveLeft = false;
    bool moveRight = false;
    double mouseX = 0.0;
    double mouseY = 0.0;
    bool leftClick       = false;
    bool rightClick      = false;
    bool toggleInventory = false;
    bool quit            = false;  // ESC
    bool rotateLeft      = false;  // Q
    bool rotateRight     = false;  // E
    bool saveKey         = false;  // Ctrl+S (raw; main edge-detects)
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
    const std::array<ItemStack, INV_SLOTS>& inventory() const { return m_inventory; }

    bool inventoryOpen() const { return m_inventoryOpen; }

    int   day()       const { return m_day; }
    float timeOfDay() const { return m_timeOfDay; } // 0.0=midnight, 0.5=noon, 1.0=midnight
    float time()      const { return m_time; }

    void setPlayerPosition(const glm::vec3& pos);
    void setTime(float t);

private:
    // Adds count items of the given type to the inventory: fills an existing
    // matching stack first, otherwise the first empty slot. Returns false if
    // there is no room (item not added).
    bool addItem(ItemType type, int count);

    Player m_player;
    std::optional<glm::ivec3> m_targetTile;

    int m_selectedSlot = 0;
    std::array<ItemStack, INV_SLOTS> m_inventory;

    bool m_inventoryOpen   = false;
    bool m_prevToggleInv   = false; // edge-detect for I key

    float m_time      = 0.0f;
    int   m_day       = 0;
    int   m_prevDay   = -1;
    float m_timeOfDay = 0.0f;
};
