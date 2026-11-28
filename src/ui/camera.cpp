#include "ui/camera.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace {
constexpr float kMinPitch = 0.05f;
constexpr float kMaxPitch = 1.55f;
constexpr float kMinDistance = 40.0f;
constexpr float kMaxDistance = 750.0f;
} // namespace

Camera::Camera() {
    setPreset(CameraPreset::TopDown);
}

void Camera::setPreset(CameraPreset preset) {
    switch (preset) {
        case CameraPreset::TopDown:
            setPositionTarget(glm::vec3(55.0f, -55.0f, 500.0f),
                              glm::vec3(0.0f, 0.0f, -30.0f),
                              glm::vec3(0.0f, 1.0f, 0.0f));
            break;
        case CameraPreset::Perspective:
            setPositionTarget(glm::vec3(220.0f, -260.0f, 240.0f),
                              glm::vec3(0.0f, 0.0f, -15.0f),
                              glm::vec3(0.0f, 0.0f, 1.0f));
            break;
        case CameraPreset::LowAngle:
            setPositionTarget(glm::vec3(180.0f, -270.0f, 95.0f),
                              glm::vec3(0.0f, 0.0f, -10.0f),
                              glm::vec3(0.0f, 0.0f, 1.0f));
            break;
        case CameraPreset::Side:
            setPositionTarget(glm::vec3(0.0f, -340.0f, 120.0f),
                              glm::vec3(0.0f, 0.0f, -10.0f),
                              glm::vec3(0.0f, 0.0f, 1.0f));
            break;
    }
}

void Camera::setPositionTarget(const glm::vec3& pos, const glm::vec3& target, const glm::vec3& up) {
    target_ = target;
    up_ = up;
    syncSphericalFromPosition(pos);
}

void Camera::syncSphericalFromPosition(const glm::vec3& pos) {
    const glm::vec3 offset = pos - target_;
    const float len = glm::length(offset);
    if (len < 1e-4f) {
        distance_ = kMinDistance;
        yaw_ = 0.0f;
        pitch_ = 1.0f;
        return;
    }
    distance_ = std::clamp(len, kMinDistance, kMaxDistance);
    const float horiz = std::hypot(offset.x, offset.y);
    pitch_ = std::clamp(std::atan2(offset.z, horiz), kMinPitch, kMaxPitch);
    yaw_ = std::atan2(offset.y, offset.x);
}

void Camera::orbit(float deltaYawRad, float deltaPitchRad) {
    yaw_ += deltaYawRad;
    pitch_ = std::clamp(pitch_ + deltaPitchRad, kMinPitch, kMaxPitch);
    up_ = glm::vec3(0.0f, 0.0f, 1.0f);
}

void Camera::pan(float deltaRight, float deltaUp) {
    const glm::vec3 pos = position();
    const glm::vec3 forward = glm::normalize(target_ - pos);
    const glm::vec3 right = glm::normalize(glm::cross(forward, up_));
    const glm::vec3 camUp = glm::cross(right, forward);
    target_ += right * deltaRight + camUp * deltaUp;
}

void Camera::zoom(float deltaDistance) {
    distance_ = std::clamp(distance_ + deltaDistance, kMinDistance, kMaxDistance);
}

void Camera::setFov(float fovDegrees) {
    fovDegrees_ = std::clamp(fovDegrees, 20.0f, 90.0f);
}

glm::vec3 Camera::position() const {
    const float cosPitch = std::cos(pitch_);
    const glm::vec3 offset(
        cosPitch * std::cos(yaw_) * distance_,
        cosPitch * std::sin(yaw_) * distance_,
        std::sin(pitch_) * distance_
    );
    return target_ + offset;
}

glm::mat4 Camera::viewMatrix() const {
    return glm::lookAt(position(), target_, up_);
}

glm::mat4 Camera::projectionMatrix(float aspect) const {
    const float safeAspect = (aspect > 0.0f) ? aspect : (16.0f / 9.0f);
    return glm::perspective(glm::radians(fovDegrees_), safeAspect, nearPlane_, farPlane_);
}

CameraState Camera::state(float aspect) const {
    CameraState out;
    out.position = position();
    out.target = target_;
    out.up = up_;
    out.fovDegrees = fovDegrees_;
    out.nearPlane = nearPlane_;
    out.farPlane = farPlane_;
    out.view = viewMatrix();
    out.projection = projectionMatrix(aspect);
    return out;
}

Ray Camera::unprojectRay(float fbX, float fbY, int fbW, int fbH) const {
    Ray result;
    if (fbW <= 0 || fbH <= 0) {
        return result;
    }
    const float aspect = static_cast<float>(fbW) / static_cast<float>(fbH);
    const glm::mat4 view = viewMatrix();
    const glm::mat4 proj = projectionMatrix(aspect);
    const glm::vec4 viewport(0.0f, 0.0f, static_cast<float>(fbW), static_cast<float>(fbH));
    const glm::vec3 nearPoint = glm::unProject(glm::vec3(fbX, fbY, 0.0f), view, proj, viewport);
    const glm::vec3 farPoint = glm::unProject(glm::vec3(fbX, fbY, 1.0f), view, proj, viewport);
    result.origin = nearPoint;
    result.direction = farPoint - nearPoint;
    return result;
}
