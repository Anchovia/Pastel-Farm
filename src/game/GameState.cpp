#include "game/GameState.h"
#include "world/World.h"
#include "game/Camera.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

#include <algorithm>

namespace {
bool canOccupy(const World& world, const glm::vec3& position) {
    const glm::ivec3 tile = world.worldToTile(position);
    return world.isWalkable(tile.x, tile.y, tile.z)
        && world.getTile(tile.x, tile.y, tile.z + 1) == TileType::AIR;
}
}

GameState::GameState() {
    m_palette = {
        ItemType::BLOCK_GRASS,  ItemType::BLOCK_DIRT,  ItemType::BLOCK_STONE,
        ItemType::BLOCK_WOOD,   ItemType::BLOCK_LEAVES, ItemType::BLOCK_WATER,
        ItemType::TOOL_HOE,     ItemType::TOOL_AXE,    ItemType::NONE,
    };
}

void GameState::update(float dt, const PlayerInput& input, const Camera& camera, World& world) {
    // Time
    m_time += dt;
    m_day = static_cast<int>(m_time / DAY_DURATION);
    m_timeOfDay = std::fmod(m_time, DAY_DURATION) / DAY_DURATION;

    // Inventory toggle (I key — edge-detect to avoid repeated triggers)
    if (input.toggleInventory && !m_prevToggleInv)
        m_inventoryOpen = !m_inventoryOpen;
    m_prevToggleInv = input.toggleInventory;

    // Hotbar selection
    if (input.selectSlot >= 0 && input.selectSlot < HOTBAR_SLOTS)
        m_selectedSlot = input.selectSlot;
    if (input.scrollDelta != 0) {
        m_selectedSlot = (m_selectedSlot + input.scrollDelta) % HOTBAR_SLOTS;
        if (m_selectedSlot < 0) m_selectedSlot += HOTBAR_SLOTS;
    }

    const glm::vec3& camPos = camera.position();
    const glm::vec3& playerPos = m_player.position();

    glm::vec2 forward = glm::normalize(glm::vec2(playerPos.x - camPos.x, playerPos.y - camPos.y));
    glm::vec2 right = glm::vec2(forward.y, -forward.x);

    glm::vec2 move{ 0.0f };
    if (input.moveForward)  move += forward;
    if (input.moveBackward) move -= forward;
    if (input.moveLeft)     move -= right;
    if (input.moveRight)    move += right;

    if (glm::length(move) > 0.0f) {
        const glm::vec2 direction = glm::normalize(move);
        const glm::vec2 delta = direction * m_player.moveSpeed() * dt;
        m_player.setFacingDirection(direction);

        glm::vec3 next = m_player.position();
        next.x += delta.x;
        if (canOccupy(world, next)) {
            m_player.moveBy({ delta.x, 0.0f });
        }

        next = m_player.position();
        next.y += delta.y;
        if (canOccupy(world, next)) {
            m_player.moveBy({ 0.0f, delta.y });
        }
    }

    if (input.windowWidth > 0 && input.windowHeight > 0) {
        // Inventory click: assign hovered item to selected hotbar slot
        if (m_inventoryOpen && input.leftClick) {
            const float W  = (float)input.windowWidth;
            const float H  = (float)input.windowHeight;
            const float gridW = INV_COLS * INV_SLOT_SIZE + (INV_COLS - 1) * INV_GAP;
            const float gridH = INV_ROWS * INV_SLOT_SIZE + (INV_ROWS - 1) * INV_GAP;
            const float ox = (W - gridW) * 0.5f - INV_PAD;
            const float oy = (H - gridH) * 0.5f - INV_PAD;
            const float mx = (float)input.mouseX;
            const float my = (float)input.mouseY;

            for (int r = 0; r < INV_ROWS; ++r) {
                for (int c = 0; c < INV_COLS; ++c) {
                    int idx = r * INV_COLS + c;
                    ItemType item = static_cast<ItemType>(idx + 1); // skip NONE(0)
                    if (item >= ItemType::COUNT) continue;

                    float sx = ox + INV_PAD + c * (INV_SLOT_SIZE + INV_GAP);
                    float sy = oy + INV_PAD + r * (INV_SLOT_SIZE + INV_GAP);
                    if (mx >= sx && mx <= sx + INV_SLOT_SIZE &&
                        my >= sy && my <= sy + INV_SLOT_SIZE) {
                        m_palette[m_selectedSlot] = item;
                    }
                }
            }
        }

        // World interaction — suppressed while inventory is open
        if (!m_inventoryOpen) {
            float ndcX = (2.0f * (float)input.mouseX) / input.windowWidth - 1.0f;
            float ndcY = (2.0f * (float)input.mouseY) / input.windowHeight - 1.0f;

            glm::mat4 invViewProj = glm::inverse(camera.viewProj());

            glm::vec4 target = invViewProj * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
            glm::vec3 rayDir = glm::normalize(glm::vec3(target / target.w) - camPos);

            if (std::abs(rayDir.z) > 1e-5f) {
                float t = -camPos.z / rayDir.z;
                if (t > 0.0f) {
                    glm::vec3 hitPoint = camPos + rayDir * t;

                    glm::ivec3 pickedTile = world.worldToTile(hitPoint);

                    // Find topmost non-AIR tile at this XY
                    for (int z = CHUNK_DEPTH - 1; z >= 0; z--) {
                        if (world.getTile(pickedTile.x, pickedTile.y, z) != TileType::AIR) {
                            pickedTile.z = z;
                            break;
                        }
                    }

                    glm::ivec3 playerTile = world.worldToTile(m_player.position());

                    glm::ivec3 delta = pickedTile - playerTile;
                    delta.x = std::clamp(delta.x, -1, 1);
                    delta.y = std::clamp(delta.y, -1, 1);
                    delta.z = std::clamp(delta.z, -1, 1);

                    glm::ivec3 finalTargetTile = playerTile + delta;

                    if (world.inBounds(finalTargetTile.x, finalTargetTile.y, finalTargetTile.z)) {
                        m_targetTile = finalTargetTile;

                        if (input.leftClick)
                            world.setTile(finalTargetTile.x, finalTargetTile.y, finalTargetTile.z, TileType::AIR);

                        if (input.rightClick) {
                            ItemType item = m_palette[m_selectedSlot];
                            if (isBlock(item)) {
                                TileType block = itemToTile(item);
                                int px = finalTargetTile.x;
                                int py = finalTargetTile.y;
                                int pz = finalTargetTile.z + 1;
                                if (world.getTile(px, py, pz) == TileType::AIR)
                                    world.setTile(px, py, pz, block);
                            }
                            // Tool actions handled in later phase (hoe -> farmland, etc.)
                        }
                    }
                    else {
                        m_targetTile = std::nullopt;
                    }
                }
            }
        }
    }
}

void GameState::updateTargetTile(World& world) {
    glm::vec3 targetPosition = m_player.position();
    targetPosition.x += m_player.facingDirection().x;
    targetPosition.y += m_player.facingDirection().y;

    const glm::ivec3 tile = world.worldToTile(targetPosition);
    if (world.inBounds(tile.x, tile.y, tile.z)) {
        m_targetTile = tile;
    } else {
        m_targetTile.reset();
    }
}
