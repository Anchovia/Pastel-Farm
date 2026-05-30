#pragma once
#include <glm/glm.hpp>

class Player {
public:
    const glm::vec3& position() const { return m_position; }
    float moveSpeed() const { return m_moveSpeed; }

    void moveBy(const glm::vec2& delta) {
        m_position.x += delta.x;
        m_position.y += delta.y;
    }

private:
    glm::vec3 m_position{0.0f, 0.0f, 1.0f};
    float     m_moveSpeed = 3.0f;
};
