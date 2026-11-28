#pragma once

#include <glm/glm.hpp>
#include "render/pool_coords.h"
#include "render/scene_state.h"
#include "render/shader_program.h"
#include "sim/wave_sim.h"

class CausticsPass {
public:
    static constexpr int kDefaultResolution = 2048;

    ~CausticsPass();

    bool init(const PoolConfig& pool, int resolution = kDefaultResolution);
    void destroy();

    void uploadHeight(const WaveSim& sim);
    void render(unsigned int waterVAO,
                int waterIndexCount,
                float meshSpacing,
                const PoolConfig& pool,
                const LightConfig& light,
                const RenderSettings& settings);

    unsigned int heightTexture() const { return heightTexture_; }
    unsigned int causticsTexture() const { return causticsTexture_; }
    glm::vec2 poolHalfExtent() const { return poolHalfExtent_; }
    float poolExtent() const { return poolHalfExtent_.x; }
    int resolution() const { return resolution_; }

private:
    void blurCausticMap();

    ShaderProgram causticsShader_;
    ShaderProgram blurShader_;
    unsigned int heightTexture_ = 0;
    unsigned int causticsFBO_ = 0;
    unsigned int causticsTexture_ = 0;
    unsigned int causticsBlurFBO_ = 0;
    unsigned int causticsBlurTexture_ = 0;
    unsigned int causticBlurVAO_ = 0;
    int resolution_ = kDefaultResolution;
    int heightTexW_ = 0;
    int heightTexH_ = 0;
    glm::vec2 poolHalfExtent_ = glm::vec2(200.0f, 200.0f);
};
