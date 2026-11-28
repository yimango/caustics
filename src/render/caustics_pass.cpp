#include "render/caustics_pass.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>

#include "render/shaders/caustics_shaders.h"

CausticsPass::~CausticsPass() {
    destroy();
}

bool CausticsPass::init(const PoolConfig& pool, int resolution) {
    destroy();

    resolution_ = resolution;
    poolHalfExtent_ = pool.halfExtent();

    if (!causticsShader_.build(kCausticVertexShader, kCausticFragmentShader, "caustics")) {
        return false;
    }
    if (!blurShader_.build(kCausticBlurVertexShader, kCausticBlurFragmentShader, "caustic_blur")) {
        destroy();
        return false;
    }

    glGenFramebuffers(1, &causticsFBO_);
    glBindFramebuffer(GL_FRAMEBUFFER, causticsFBO_);

    glGenTextures(1, &causticsTexture_);
    glBindTexture(GL_TEXTURE_2D, causticsTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, resolution_, resolution_, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, causticsTexture_, 0);

    GLenum buf = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &buf);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Caustics FBO incomplete\n";
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        destroy();
        return false;
    }

    glGenFramebuffers(1, &causticsBlurFBO_);
    glBindFramebuffer(GL_FRAMEBUFFER, causticsBlurFBO_);

    glGenTextures(1, &causticsBlurTexture_);
    glBindTexture(GL_TEXTURE_2D, causticsBlurTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, resolution_, resolution_, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, causticsBlurTexture_, 0);
    glDrawBuffers(1, &buf);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Caustics blur FBO incomplete\n";
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        destroy();
        return false;
    }

    glGenVertexArrays(1, &causticBlurVAO_);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    heightTexW_ = pool.gridW;
    heightTexH_ = pool.gridH;
    std::vector<float> zeroField(static_cast<std::size_t>(heightTexW_) * static_cast<std::size_t>(heightTexH_), 0.0f);

    glGenTextures(1, &heightTexture_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, heightTexture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, heightTexW_, heightTexH_, 0, GL_RED, GL_FLOAT, zeroField.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return true;
}

void CausticsPass::destroy() {
    if (heightTexture_ != 0) {
        glDeleteTextures(1, &heightTexture_);
        heightTexture_ = 0;
    }
    if (causticsFBO_ != 0) {
        glDeleteFramebuffers(1, &causticsFBO_);
        causticsFBO_ = 0;
    }
    if (causticsTexture_ != 0) {
        glDeleteTextures(1, &causticsTexture_);
        causticsTexture_ = 0;
    }
    if (causticsBlurFBO_ != 0) {
        glDeleteFramebuffers(1, &causticsBlurFBO_);
        causticsBlurFBO_ = 0;
    }
    if (causticsBlurTexture_ != 0) {
        glDeleteTextures(1, &causticsBlurTexture_);
        causticsBlurTexture_ = 0;
    }
    if (causticBlurVAO_ != 0) {
        glDeleteVertexArrays(1, &causticBlurVAO_);
        causticBlurVAO_ = 0;
    }
    causticsShader_.destroy();
    blurShader_.destroy();
    heightTexW_ = 0;
    heightTexH_ = 0;
}

void CausticsPass::uploadHeight(const WaveSim& sim) {
    if (heightTexture_ == 0 || sim.width <= 0 || sim.height <= 0 || sim.height_current.empty()) {
        return;
    }
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, heightTexture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (sim.width != heightTexW_ || sim.height != heightTexH_) {
        heightTexW_ = sim.width;
        heightTexH_ = sim.height;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, heightTexW_, heightTexH_, 0, GL_RED, GL_FLOAT, sim.height_current.data());
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, heightTexW_, heightTexH_, GL_RED, GL_FLOAT, sim.height_current.data());
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void CausticsPass::blurCausticMap() {
    glDisable(GL_BLEND);
    blurShader_.use();
    glBindVertexArray(causticBlurVAO_);
    blurShader_.setInt("source", 0);
    glActiveTexture(GL_TEXTURE0);

    const float invRes = 1.0f / static_cast<float>(resolution_);
    glBindFramebuffer(GL_FRAMEBUFFER, causticsBlurFBO_);
    glBindTexture(GL_TEXTURE_2D, causticsTexture_);
    blurShader_.setVec2("direction", glm::vec2(invRes, 0.0f));
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindFramebuffer(GL_FRAMEBUFFER, causticsFBO_);
    glBindTexture(GL_TEXTURE_2D, causticsBlurTexture_);
    blurShader_.setVec2("direction", glm::vec2(0.0f, invRes));
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void CausticsPass::render(unsigned int waterVAO,
                          int waterIndexCount,
                          float meshSpacing,
                          const PoolConfig& pool,
                          const LightConfig& light,
                          const RenderSettings& settings) {
    poolHalfExtent_ = pool.halfExtent();

    glBindFramebuffer(GL_FRAMEBUFFER, causticsFBO_);
    glViewport(0, 0, resolution_, resolution_);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    const glm::mat4 model(1.0f);
    const glm::mat4 causticsView =
        glm::lookAt(glm::vec3(0.0f, 0.0f, 80.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 causticsProj =
        glm::ortho(-poolHalfExtent_.x, poolHalfExtent_.x, -poolHalfExtent_.y, poolHalfExtent_.y, 0.1f, 300.0f);

    causticsShader_.use();
    causticsShader_.setMat4("model", model);
    causticsShader_.setMat4("view", causticsView);
    causticsShader_.setMat4("projection", causticsProj);
    causticsShader_.setVec3("lightPos", light.position);
    causticsShader_.setFloat("bottomZ", pool.bottomZ);
    causticsShader_.setFloat("waterIOR", settings.waterIOR);
    causticsShader_.setFloat("airIOR", settings.airIOR);
    causticsShader_.setFloat("heightScale", pool.meshHeightScale);
    causticsShader_.setVec2("poolHalfExtent", poolHalfExtent_);
    causticsShader_.setVec2("gridSize", glm::vec2(static_cast<float>(pool.gridW), static_cast<float>(pool.gridH)));
    causticsShader_.setFloat("meshSpacing", meshSpacing);
    causticsShader_.setInt("heightTex", 1);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, heightTexture_);

    glBindVertexArray(waterVAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(waterIndexCount), GL_UNSIGNED_INT, nullptr);

    blurCausticMap();

    glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
