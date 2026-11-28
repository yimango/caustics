#pragma once

#include "render/scene_state.h"
#include "render/shader_program.h"

class SkyboxRenderer {
public:
    ~SkyboxRenderer();

    bool init();
    void destroy();

    void render(const CameraState& camera, const LightConfig& light) const;

private:
    ShaderProgram shader_;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
};
