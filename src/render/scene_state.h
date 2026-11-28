#pragma once

#include <glm/glm.hpp>

struct LightConfig {
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 100.0f);
    glm::vec3 color = glm::vec3(1.0f, 0.98f, 0.94f);
    float intensity = 1.0f;
};

struct RenderSettings {
    float waterIOR = 1.33f;
    float airIOR = 1.0f;
    float dispersion = 0.0f;
    float causticStrength = 1.15f;
    float turbidity = 0.0f;
    float exposure = 1.0f;
    bool showUI = true;
};

struct SceneTargets {
    unsigned int colorTex = 0;
    unsigned int depthTex = 0;
    unsigned int fbo = 0;
    int width = 0;
    int height = 0;

    bool valid() const {
        return fbo != 0 && colorTex != 0 && depthTex != 0 && width > 0 && height > 0;
    }
};

struct CameraState {
    glm::vec3 position = glm::vec3(55.0f, -55.0f, 500.0f);
    glm::vec3 target = glm::vec3(0.0f, 0.0f, -30.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    float fovDegrees = 50.0f;
    float nearPlane = 0.1f;
    float farPlane = 800.0f;
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
};
