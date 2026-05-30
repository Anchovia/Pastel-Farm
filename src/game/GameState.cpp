#include "game/GameState.h"
#include "world/World.h"

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
        const glm::vec2 direction = glm::normalize(move);
        const glm::vec2 delta = direction * m_player.moveSpeed() * dt;
        m_player.setFacingDirection(direction);

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

    // 마우스 피킹 (Raycasting) 로직
    if (input.windowWidth > 0 && input.windowHeight > 0) {
        // NDC 좌표로 변환 (-1.0 ~ 1.0)
        float ndcX = (2.0f * (float)input.mouseX) / input.windowWidth - 1.0f;
        float ndcY = (2.0f * (float)input.mouseY) / input.windowHeight - 1.0f;

        // VulkanContext와 동일한 카메라 행렬 계산 (임시 복제)
        // 나중에 Camera 클래스로 분리하면 더 깔끔해집니다.
        float orbitDistance = 14.0f;
        float orbitPitch = 45.0f;
        float rad = glm::radians(cameraAngleDegrees);
        float pitch = glm::radians(orbitPitch);

        glm::vec3 orbitTarget = { m_player.position().x, m_player.position().y, 0.0f };
        glm::vec3 camPos = orbitTarget + glm::vec3{
            orbitDistance * glm::cos(pitch) * glm::cos(rad),
            orbitDistance * glm::cos(pitch) * glm::sin(rad),
            orbitDistance * glm::sin(pitch)
        };

        glm::mat4 view = glm::lookAt(camPos, orbitTarget, glm::vec3{ 0.0f, 0.0f, 1.0f });
        glm::mat4 proj = glm::perspective(glm::radians(45.0f),
            (float)input.windowWidth / input.windowHeight, 0.1f, 100.0f);
        proj[1][1] *= -1; // Vulkan Y축 반전

        // 역행렬 계산
        glm::mat4 invViewProj = glm::inverse(proj * view);

        // Ray 방향 벡터 산출
        glm::vec4 target = invViewProj * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
        glm::vec3 rayDir = glm::normalize(glm::vec3(target / target.w) - camPos);

        // Z=0 평면과의 교차점 계산
        if (std::abs(rayDir.z) > 1e-5f) {
            float t = -camPos.z / rayDir.z;
            if (t > 0.0f) {
                glm::vec3 hitPoint = camPos + rayDir * t;

                // 1. 마우스가 실제로 가리키는 타일과 플레이어가 서 있는 타일을 각각 구함
                glm::ivec2 pickedTile = world.worldToTile(hitPoint);
                glm::ivec2 playerTile = world.worldToTile(m_player.position());

                // 2. 플레이어 타일 기준으로 얼마나 떨어져 있는지 거리(Delta) 계산
                glm::ivec2 delta = pickedTile - playerTile;

                // 3. 거리를 상하좌우 대각선 최대 1칸(-1 ~ 1)으로 제한 (Clamp)
                delta.x = std::clamp(delta.x, -1, 1);
                delta.y = std::clamp(delta.y, -1, 1);

                // 4. 제한된 거리를 적용한 최종 타겟 타일 계산
                glm::ivec2 finalTargetTile = playerTile + delta;

                // 5. 월드 범위 안에 마우스가 있으면 노란색 셀렉터를 그 위치로 이동
                if (world.inBounds(finalTargetTile.x, finalTargetTile.y)) {
                    m_targetTile = finalTargetTile;

                    // 만약 클릭까지 했다면? (씨앗 심기, 땅 파기 등)
                    if (input.leftClick) {
                        // std::cout << "Action on Tile: " << finalTargetTile.x << ", " << finalTargetTile.y << "\n";
                    }
                }
                else {
                    m_targetTile = std::nullopt; // 맵 밖이면 셀렉터 숨김
                }
            }
        }
    }

    // updateTargetTile(world);
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
