#pragma once

#include <glm/glm.hpp>
#include "render/scene_state.h"

enum class CameraPreset {
    TopDown,
    Perspective,
    LowAngle,
    Side
};

struct Ray {
    glm::vec3 origin = glm::vec3(0.0f);
    glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);
};

class Camera {
public:
    Camera();

    void setPreset(CameraPreset preset);
    void orbit(float deltaYawRad, float deltaPitchRad);
    void pan(float deltaRight, float deltaUp);
    void zoom(float deltaDistance);
    void setPositionTarget(const glm::vec3& position,
                           const glm::vec3& target,
                           const glm::vec3& up = glm::vec3(0.0f, 0.0f, 1.0f));
    void setPositionTargetAutoUp(const glm::vec3& position, const glm::vec3& target);
    void setFov(float fovDegrees);

    glm::vec3 position() const;
    glm::vec3 target() const { return target_; }
    glm::vec3 up() const { return up_; }
    float yaw() const { return yaw_; }
    float pitch() const { return pitch_; }
    float distance() const { return distance_; }
    float fovDegrees() const { return fovDegrees_; }

    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix(float aspect) const;
    CameraState state(float aspect) const;

    Ray unprojectRay(float fbX, float fbY, int fbW, int fbH) const;

private:
    void syncSphericalFromPosition(const glm::vec3& position);

    glm::vec3 target_ = glm::vec3(0.0f, 0.0f, -30.0f);
    glm::vec3 up_ = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw_ = 0.0f;
    float pitch_ = 1.425f;
    float distance_ = 535.677f;
    float fovDegrees_ = 50.0f;
    float nearPlane_ = 0.1f;
    float farPlane_ = 1600.0f;
};
