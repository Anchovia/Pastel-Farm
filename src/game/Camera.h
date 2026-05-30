// src/game/Camera.h
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    Camera(float fov, float aspect, float nearClip, float farClip)
        : m_fov(fov), m_aspect(aspect), m_near(nearClip), m_far(farClip) {
        updateProjection();
    }

    void setAspectRatio(float aspect) {
        m_aspect = aspect;
        updateProjection();
    }

    // 매 프레임 타겟(플레이어) 위치와 회전각을 받아 카메라 갱신
    void update(const glm::vec3& target, float orbitAngleDegrees) {
        float rad = glm::radians(orbitAngleDegrees);
        float pitch = glm::radians(m_orbitPitch);

        m_position = target + glm::vec3{
            m_orbitDistance * glm::cos(pitch) * glm::cos(rad),
            m_orbitDistance * glm::cos(pitch) * glm::sin(rad),
            m_orbitDistance * glm::sin(pitch)
        };

        m_view = glm::lookAt(m_position, target, glm::vec3{ 0.0f, 0.0f, 1.0f });
    }

    const glm::mat4& view() const { return m_view; }
    const glm::mat4& proj() const { return m_proj; }
    glm::mat4 viewProj() const { return m_proj * m_view; }
    const glm::vec3& position() const { return m_position; }

private:
    void updateProjection() {
        m_proj = glm::perspective(glm::radians(m_fov), m_aspect, m_near, m_far);
        m_proj[1][1] *= -1; // Vulkan Y축 반전
    }

    glm::mat4 m_view{ 1.0f };
    glm::mat4 m_proj{ 1.0f };
    glm::vec3 m_position{ 0.0f, 0.0f, 0.0f };

    float m_fov;
    float m_aspect;
    float m_near;
    float m_far;

    float m_orbitDistance = 14.0f;
    float m_orbitPitch = 45.0f;
};