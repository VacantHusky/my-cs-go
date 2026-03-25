#pragma once

#include "util/MathTypes.h"

#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif

#ifndef GLM_FORCE_RIGHT_HANDED
#define GLM_FORCE_RIGHT_HANDED
#endif
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace mycsg::util {

inline float degreesToRadians(const float degrees) {
    return degrees * 0.01745329251994329577f;
}

inline Vec3 cameraForwardVector(const float yawRadians, const float pitchRadians) {
    const float cosPitch = std::cos(pitchRadians);
    return {
        std::cos(yawRadians) * cosPitch,
        std::sin(pitchRadians),
        std::sin(yawRadians) * cosPitch,
    };
}

inline float mapEditorOrthoYawRadians() {
    return degreesToRadians(45.0f);
}

inline float mapEditorOrthoPitchRadians() {
    return degreesToRadians(-35.264389f);
}

inline float mapEditorOrthoEyeDistance(const float orthoSpan) {
    return std::max(18.0f, orthoSpan * 1.9f);
}

inline Vec3 mapEditorOrthoEyePosition(const Vec3& focusPosition, const float orthoSpan) {
    const Vec3 forward = cameraForwardVector(mapEditorOrthoYawRadians(), mapEditorOrthoPitchRadians());
    const float distance = mapEditorOrthoEyeDistance(orthoSpan);
    return {
        focusPosition.x - forward.x * distance,
        focusPosition.y - forward.y * distance,
        focusPosition.z - forward.z * distance,
    };
}

inline glm::mat4 buildMapEditorProjectionMatrix(const bool orthoView,
                                                const float width,
                                                const float height,
                                                const float orthoSpan) {
    constexpr float kPerspectiveFovRadians = 1.08f;
    constexpr float kPerspectiveNearPlane = 0.05f;
    constexpr float kPerspectiveFarPlane = 192.0f;
    constexpr float kOrthoNearPlane = 0.05f;
    constexpr float kOrthoFarPlane = 384.0f;

    const float safeHeight = std::max(1.0f, height);
    const float aspect = width / safeHeight;
    glm::mat4 projection(1.0f);
    if (orthoView) {
        const float span = std::max(4.0f, orthoSpan);
        projection = glm::orthoRH_ZO(
            -span * aspect,
            span * aspect,
            -span,
            span,
            kOrthoNearPlane,
            kOrthoFarPlane);
    } else {
        projection = glm::perspectiveRH_ZO(
            kPerspectiveFovRadians,
            aspect,
            kPerspectiveNearPlane,
            kPerspectiveFarPlane);
    }

    projection[1][1] *= -1.0f;
    return projection;
}

inline glm::mat4 buildMapEditorViewMatrix(const bool orthoView,
                                          const Vec3& cameraPosition,
                                          const float yawRadians,
                                          const float pitchRadians,
                                          const float orthoSpan) {
    if (orthoView) {
        const Vec3 eye = mapEditorOrthoEyePosition(cameraPosition, orthoSpan);
        return glm::lookAtRH(
            glm::vec3(eye.x, eye.y, eye.z),
            glm::vec3(cameraPosition.x, cameraPosition.y, cameraPosition.z),
            glm::vec3(0.0f, 1.0f, 0.0f));
    }

    const Vec3 forward = cameraForwardVector(yawRadians, pitchRadians);
    return glm::lookAtRH(
        glm::vec3(cameraPosition.x, cameraPosition.y, cameraPosition.z),
        glm::vec3(
            cameraPosition.x + forward.x,
            cameraPosition.y + forward.y,
            cameraPosition.z + forward.z),
        glm::vec3(0.0f, 1.0f, 0.0f));
}

}  // namespace mycsg::util
