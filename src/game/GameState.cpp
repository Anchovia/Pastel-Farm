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
    const glm::ivec2 tile = world.worldToTile(position);
    return world.isWalkable(tile.x, tile.y);
}
}

void GameState::update(float dt, const PlayerInput& input, const Camera& camera, const World& world) {
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
        float ndcX = (2.0f * (float)input.mouseX) / input.windowWidth - 1.0f;
        float ndcY = (2.0f * (float)input.mouseY) / input.windowHeight - 1.0f;

        glm::mat4 invViewProj = glm::inverse(camera.viewProj());

        glm::vec4 target = invViewProj * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
        glm::vec3 rayDir = glm::normalize(glm::vec3(target / target.w) - camPos);

        if (std::abs(rayDir.z) > 1e-5f) {
            float t = -camPos.z / rayDir.z;
            if (t > 0.0f) {
                glm::vec3 hitPoint = camPos + rayDir * t;

                // 마우스가 가리키는 타일과 플레이어 타일 구하기
                glm::ivec2 pickedTile = world.worldToTile(hitPoint);
                glm::ivec2 playerTile = world.worldToTile(m_player.position());

                // 사거리 제한 계산 (반경 1칸으로 Clamp)
                glm::ivec2 delta = pickedTile - playerTile;
                delta.x = std::clamp(delta.x, -1, 1);
                delta.y = std::clamp(delta.y, -1, 1);

                glm::ivec2 finalTargetTile = playerTile + delta;

                // 월드 범위 안이면 셀렉터 위치 갱신
                if (world.inBounds(finalTargetTile.x, finalTargetTile.y)) {
                    m_targetTile = finalTargetTile;

                    if (input.leftClick) {
                        // 추후 이곳에 상호작용(땅 파기, 블록 설치 등) 로직 추가 예정
                    }
                }
                else {
                    m_targetTile = std::nullopt;
                }
            }
        }
    }
}

void GameState::updateTargetTile(const World& world) {
    glm::vec3 targetPosition = m_player.position();
    targetPosition.x += m_player.facingDirection().x;
    targetPosition.y += m_player.facingDirection().y;

    const glm::ivec2 tile = world.worldToTile(targetPosition);
    if (world.inBounds(tile.x, tile.y)) {
        m_targetTile = tile;
    } else {
        m_targetTile.reset();
    }
}
