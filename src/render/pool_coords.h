#pragma once

#include <glm/glm.hpp>

struct PoolConfig {
    int gridW = 400;
    int gridH = 400;
    float waterScale = 1.0f;
    float bottomZ = -30.0f;
    float lipZ = 18.0f;
    float meshHeightScale = 8.0f;

    float halfWidth() const { return 0.5f * static_cast<float>(gridW) * waterScale; }
    float halfHeight() const { return 0.5f * static_cast<float>(gridH) * waterScale; }
    glm::vec2 halfExtent() const { return glm::vec2(halfWidth(), halfHeight()); }

    float gridToWorldX(float gx) const {
        const float hw = halfWidth();
        if (gridW <= 1) {
            return 0.0f;
        }
        return -hw + (gx / static_cast<float>(gridW - 1)) * (2.0f * hw);
    }

    float gridToWorldY(float gy) const {
        const float hh = halfHeight();
        if (gridH <= 1) {
            return 0.0f;
        }
        return -hh + (gy / static_cast<float>(gridH - 1)) * (2.0f * hh);
    }

    float worldToGridX(float x) const {
        const float hw = halfWidth();
        if (hw <= 0.0f || gridW <= 1) {
            return 0.0f;
        }
        return ((x + hw) / (2.0f * hw)) * static_cast<float>(gridW - 1);
    }

    float worldToGridY(float y) const {
        const float hh = halfHeight();
        if (hh <= 0.0f || gridH <= 1) {
            return 0.0f;
        }
        return ((y + hh) / (2.0f * hh)) * static_cast<float>(gridH - 1);
    }

    glm::vec2 gridToWorld(float gx, float gy) const {
        return glm::vec2(gridToWorldX(gx), gridToWorldY(gy));
    }

    glm::vec2 worldToGrid(float x, float y) const {
        return glm::vec2(worldToGridX(x), worldToGridY(y));
    }

    glm::vec2 worldToUV(float x, float y) const {
        const float hw = halfWidth();
        const float hh = halfHeight();
        return glm::vec2((x + hw) / (2.0f * hw), (y + hh) / (2.0f * hh));
    }

    float worldStepX() const {
        return (gridW > 1) ? (2.0f * halfWidth()) / static_cast<float>(gridW - 1) : waterScale;
    }

    float worldStepY() const {
        return (gridH > 1) ? (2.0f * halfHeight()) / static_cast<float>(gridH - 1) : waterScale;
    }
};
