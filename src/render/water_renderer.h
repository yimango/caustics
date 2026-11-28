#pragma once

#include "render/pool_coords.h"
#include "render/scene_state.h"
#include "render/shader_program.h"

class WaterRenderer {
public:
    ~WaterRenderer();

    bool init(const PoolConfig& pool, int meshSubdiv = 2);
    void destroy();

    void render(const CameraState& camera,
                const PoolConfig& pool,
                const LightConfig& light,
                const RenderSettings& settings,
                unsigned int heightTexture,
                const SceneTargets* sceneTargets = nullptr) const;

    unsigned int vao() const { return vao_; }
    int indexCount() const { return indexCount_; }
    float meshSpacing() const { return meshSpacing_; }
    int axisX() const { return axisX_; }
    int axisY() const { return axisY_; }

private:
    static constexpr float kBorderOverlap = 0.05f;

    ShaderProgram shader_;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int ebo_ = 0;
    int indexCount_ = 0;
    int axisX_ = 0;
    int axisY_ = 0;
    float meshSpacing_ = 0.5f;
};
