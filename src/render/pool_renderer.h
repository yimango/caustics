#pragma once

#include "render/pool_coords.h"
#include "render/scene_state.h"
#include "render/shader_program.h"

class PoolRenderer {
public:
    ~PoolRenderer();

    bool init(const PoolConfig& pool);
    void destroy();

    void render(const CameraState& camera,
                const PoolConfig& pool,
                const LightConfig& light,
                const RenderSettings& settings,
                unsigned int causticsTexture,
                unsigned int heightTexture = 0) const;

private:
    ShaderProgram shader_;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int ebo_ = 0;
    int floorIndexCount_ = 0;
    int totalIndexCount_ = 0;
};
