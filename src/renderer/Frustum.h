#pragma once
#include <glm/glm.hpp>
#include <array>

struct Frustum {
    std::array<glm::vec4, 6> planes; // ax + by + cz + d >= 0 이면 내부

    // Gribb & Hartmann 방법 — viewProj 행렬에서 6개 평면 직접 추출
    static Frustum extractFrom(const glm::mat4& m) {
        // GLM은 column-major: m[col][row]
        auto row = [&](int r) {
            return glm::vec4(m[0][r], m[1][r], m[2][r], m[3][r]);
        };
        Frustum f;
        f.planes[0] = row(3) + row(0); // left
        f.planes[1] = row(3) - row(0); // right
        f.planes[2] = row(3) + row(1); // bottom
        f.planes[3] = row(3) - row(1); // top
        f.planes[4] = row(3) + row(2); // near
        f.planes[5] = row(3) - row(2); // far
        return f;
    }

    // AABB가 프러스텀 안에 있으면 true
    // 평면 하나라도 AABB 전체가 바깥이면 false (컬링)
    bool containsAABB(const glm::vec3& min, const glm::vec3& max) const {
        for (const auto& plane : planes) {
            glm::vec3 n(plane);
            // 평면 법선 방향으로 가장 멀리 있는 꼭짓점 (positive vertex)
            glm::vec3 p = {
                n.x >= 0.0f ? max.x : min.x,
                n.y >= 0.0f ? max.y : min.y,
                n.z >= 0.0f ? max.z : min.z
            };
            if (glm::dot(n, p) + plane.w < 0.0f) return false;
        }
        return true;
    }
};
