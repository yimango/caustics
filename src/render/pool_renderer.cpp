#include "render/pool_renderer.h"

#include <glad/glad.h>
#include <vector>

#include "render/shaders/pool_shaders.h"

namespace {

void appendVertex(std::vector<float>& vertices, float x, float y, float z, float nx, float ny, float nz) {
    vertices.push_back(x);
    vertices.push_back(y);
    vertices.push_back(z);
    vertices.push_back(nx);
    vertices.push_back(ny);
    vertices.push_back(nz);
}

void appendQuad(std::vector<unsigned int>& indices,
                unsigned int i0,
                unsigned int i1,
                unsigned int i2,
                unsigned int i3) {
    indices.push_back(i0);
    indices.push_back(i1);
    indices.push_back(i2);
    indices.push_back(i2);
    indices.push_back(i3);
    indices.push_back(i0);
}

} // namespace

PoolRenderer::~PoolRenderer() {
    destroy();
}

bool PoolRenderer::init(const PoolConfig& pool) {
    destroy();

    if (!shader_.build(kPoolVertexShader, kPoolFragmentShader, "pool")) {
        return false;
    }

    const float bottomZ = pool.bottomZ;
    const float lipZ = pool.lipZ;
    const float hw = pool.halfWidth();
    const float hh = pool.halfHeight();

    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    // Floor (+Z normal)
    appendVertex(vertices, -hw, -hh, bottomZ, 0.0f, 0.0f, 1.0f);
    appendVertex(vertices, hw, -hh, bottomZ, 0.0f, 0.0f, 1.0f);
    appendVertex(vertices, hw, hh, bottomZ, 0.0f, 0.0f, 1.0f);
    appendVertex(vertices, -hw, hh, bottomZ, 0.0f, 0.0f, 1.0f);
    appendQuad(indices, 0, 1, 2, 3);
    floorIndexCount_ = static_cast<int>(indices.size());

    // South wall (+Y inward normal)
    const unsigned int south = static_cast<unsigned int>(vertices.size() / 6);
    appendVertex(vertices, -hw, -hh, bottomZ, 0.0f, 1.0f, 0.0f);
    appendVertex(vertices, hw, -hh, bottomZ, 0.0f, 1.0f, 0.0f);
    appendVertex(vertices, hw, -hh, lipZ, 0.0f, 1.0f, 0.0f);
    appendVertex(vertices, -hw, -hh, lipZ, 0.0f, 1.0f, 0.0f);
    appendQuad(indices, south, south + 1, south + 2, south + 3);

    // East wall (-X inward normal)
    const unsigned int east = static_cast<unsigned int>(vertices.size() / 6);
    appendVertex(vertices, hw, -hh, bottomZ, -1.0f, 0.0f, 0.0f);
    appendVertex(vertices, hw, hh, bottomZ, -1.0f, 0.0f, 0.0f);
    appendVertex(vertices, hw, hh, lipZ, -1.0f, 0.0f, 0.0f);
    appendVertex(vertices, hw, -hh, lipZ, -1.0f, 0.0f, 0.0f);
    appendQuad(indices, east, east + 1, east + 2, east + 3);

    // North wall (-Y inward normal)
    const unsigned int north = static_cast<unsigned int>(vertices.size() / 6);
    appendVertex(vertices, hw, hh, bottomZ, 0.0f, -1.0f, 0.0f);
    appendVertex(vertices, -hw, hh, bottomZ, 0.0f, -1.0f, 0.0f);
    appendVertex(vertices, -hw, hh, lipZ, 0.0f, -1.0f, 0.0f);
    appendVertex(vertices, hw, hh, lipZ, 0.0f, -1.0f, 0.0f);
    appendQuad(indices, north, north + 1, north + 2, north + 3);

    // West wall (+X inward normal)
    const unsigned int west = static_cast<unsigned int>(vertices.size() / 6);
    appendVertex(vertices, -hw, hh, bottomZ, 1.0f, 0.0f, 0.0f);
    appendVertex(vertices, -hw, -hh, bottomZ, 1.0f, 0.0f, 0.0f);
    appendVertex(vertices, -hw, -hh, lipZ, 1.0f, 0.0f, 0.0f);
    appendVertex(vertices, -hw, hh, lipZ, 1.0f, 0.0f, 0.0f);
    appendQuad(indices, west, west + 1, west + 2, west + 3);

    totalIndexCount_ = static_cast<int>(indices.size());

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    return true;
}

void PoolRenderer::destroy() {
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
    floorIndexCount_ = 0;
    totalIndexCount_ = 0;
}

void PoolRenderer::render(const CameraState& camera,
                          const PoolConfig& pool,
                          const LightConfig& /*light*/,
                          const RenderSettings& /*settings*/,
                          unsigned int causticsTexture) const {
    const glm::mat4 model(1.0f);
    shader_.use();
    shader_.setMat4("model", model);
    shader_.setMat4("view", camera.view);
    shader_.setMat4("projection", camera.projection);
    shader_.setInt("causticsTexture", 0);
    shader_.setVec2("poolHalfExtent", pool.halfExtent());
    shader_.setFloat("bottomZ", pool.bottomZ);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, causticsTexture);

    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, floorIndexCount_, GL_UNSIGNED_INT, nullptr);
    glDrawElements(GL_TRIANGLES,
                   totalIndexCount_ - floorIndexCount_,
                   GL_UNSIGNED_INT,
                   reinterpret_cast<void*>(static_cast<std::size_t>(floorIndexCount_) * sizeof(unsigned int)));
}
