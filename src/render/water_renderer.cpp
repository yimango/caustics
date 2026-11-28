#include "render/water_renderer.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <cstddef>
#include <vector>

#include "render/shaders/water_shaders.h"

WaterRenderer::~WaterRenderer() {
    destroy();
}

bool WaterRenderer::init(const PoolConfig& pool, int meshSubdiv) {
    destroy();

    if (!shader_.build(kWaterVertexShader, kWaterFragmentShader, "water")) {
        return false;
    }

    axisX_ = (pool.gridW - 1) * meshSubdiv + 1;
    axisY_ = (pool.gridH - 1) * meshSubdiv + 1;
    meshSpacing_ = (axisX_ > 1) ? (2.0f * pool.halfWidth()) / static_cast<float>(axisX_ - 1) : pool.waterScale;

    const float hw = pool.halfWidth();
    const float hh = pool.halfHeight();

    std::vector<float> vertices(static_cast<std::size_t>(axisX_) * static_cast<std::size_t>(axisY_) * 3);
    std::size_t v = 0;
    for (int j = 0; j < axisY_; ++j) {
        float wy = 0.0f;
        if (j == 0) {
            wy = -hh;
        } else if (j == axisY_ - 1) {
            wy = hh;
        } else {
            const float gy = static_cast<float>(j) * static_cast<float>(pool.gridH - 1) / static_cast<float>(axisY_ - 1);
            wy = pool.gridToWorldY(gy);
        }
        for (int i = 0; i < axisX_; ++i) {
            float wx = 0.0f;
            if (i == 0) {
                wx = -hw;
            } else if (i == axisX_ - 1) {
                wx = hw;
            } else {
                const float gx = static_cast<float>(i) * static_cast<float>(pool.gridW - 1) / static_cast<float>(axisX_ - 1);
                wx = pool.gridToWorldX(gx);
            }
            vertices[v++] = wx;
            vertices[v++] = wy;
            vertices[v++] = 0.0f;
        }
    }

    std::vector<unsigned int> indices;
    indices.reserve(static_cast<std::size_t>(axisX_ - 1) * static_cast<std::size_t>(axisY_ - 1) * 6);
    for (int j = 0; j < axisY_ - 1; ++j) {
        for (int i = 0; i < axisX_ - 1; ++i) {
            const unsigned int v00 = static_cast<unsigned int>(j * axisX_ + i);
            const unsigned int v10 = v00 + 1;
            const unsigned int v01 = static_cast<unsigned int>((j + 1) * axisX_ + i);
            const unsigned int v11 = v01 + 1;

            indices.push_back(v00);
            indices.push_back(v10);
            indices.push_back(v11);

            indices.push_back(v00);
            indices.push_back(v11);
            indices.push_back(v01);
        }
    }
    indexCount_ = static_cast<int>(indices.size());

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    return true;
}

void WaterRenderer::destroy() {
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (ebo_ != 0) {
        glDeleteBuffers(1, &ebo_);
        ebo_ = 0;
    }
    shader_.destroy();
    indexCount_ = 0;
    axisX_ = 0;
    axisY_ = 0;
}

void WaterRenderer::render(const CameraState& camera,
                           const PoolConfig& pool,
                           const LightConfig& light,
                           const RenderSettings& settings,
                           unsigned int heightTexture,
                           const SceneTargets* sceneTargets) const {
    const glm::mat4 model(1.0f);
    const glm::mat4 viewProj = camera.projection * camera.view;
    const glm::mat4 invViewProj = glm::inverse(viewProj);
    const bool hasScene = (sceneTargets != nullptr && sceneTargets->valid());

    if (hasScene) {
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    } else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }

    shader_.use();
    shader_.setMat4("model", model);
    shader_.setMat4("view", camera.view);
    shader_.setMat4("projection", camera.projection);
    shader_.setMat4("viewProj", viewProj);
    shader_.setMat4("invViewProj", invViewProj);

    shader_.setVec3("lightPos", light.position);
    shader_.setVec3("lightColor", light.color);
    shader_.setFloat("lightIntensity", light.intensity);
    shader_.setVec3("viewPos", camera.position);
    shader_.setVec2("nearFar", glm::vec2(camera.nearPlane, camera.farPlane));

    shader_.setFloat("heightScale", pool.meshHeightScale);
    shader_.setVec2("poolHalfExtent", pool.halfExtent());
    shader_.setVec2("gridSize", glm::vec2(static_cast<float>(pool.gridW), static_cast<float>(pool.gridH)));
    shader_.setFloat("bottomZ", pool.bottomZ);
    shader_.setFloat("lipZ", pool.lipZ);
    shader_.setFloat("borderOverlap", kBorderOverlap);

    shader_.setFloat("waterIOR", settings.waterIOR);
    shader_.setFloat("airIOR", settings.airIOR);
    shader_.setFloat("dispersion", settings.dispersion);
    shader_.setFloat("turbidity", settings.turbidity);
    shader_.setFloat("exposure", settings.exposure);

    shader_.setInt("heightTex", 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, heightTexture);

    shader_.setInt("hasSceneTargets", hasScene ? 1 : 0);
    if (hasScene) {
        shader_.setVec2("sceneResolution",
                        glm::vec2(static_cast<float>(sceneTargets->width),
                                  static_cast<float>(sceneTargets->height)));
        shader_.setInt("sceneColorTex", 2);
        shader_.setInt("sceneDepthTex", 3);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, sceneTargets->colorTex);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, sceneTargets->depthTex);
    } else {
        shader_.setVec2("sceneResolution", glm::vec2(1.0f, 1.0f));
    }

    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount_), GL_UNSIGNED_INT, nullptr);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
}
