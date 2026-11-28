#include "sim/wave_sim.h"

#include <cmath>
#include <cstddef>

void WaveSim::init(int w, int h) {
    width = w;
    height = h;
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    height_current.assign(count, 0.0f);
    height_prev.assign(count, 0.0f);
    height_next.assign(count, 0.0f);
}

void WaveSim::step() {
    if (width <= 0 || height <= 0) {
        return;
    }

    const float coeff = (c * c * dt * dt) / (dx * dx);
    const float dampFactor = 1.0f - damping;

    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            const int i = idx(x, y);
            const float cur = height_current[i];
            const float prev = height_prev[i];
            const float laplacian =
                height_current[idx(x + 1, y)] +
                height_current[idx(x - 1, y)] +
                height_current[idx(x, y + 1)] +
                height_current[idx(x, y - 1)] -
                4.0f * cur;
            height_next[i] = dampFactor * (2.0f * cur - prev) + coeff * laplacian;
        }
    }

    // Zero border cells for FixedDirichlet (default Track 0 boundary behavior).
    for (int x = 0; x < width; ++x) {
        height_next[idx(x, 0)] = 0.0f;
        height_next[idx(x, height - 1)] = 0.0f;
    }
    for (int y = 0; y < height; ++y) {
        height_next[idx(0, y)] = 0.0f;
        height_next[idx(width - 1, y)] = 0.0f;
    }

    height_prev.swap(height_current);
    height_current.swap(height_next);
}

void WaveSim::addDisturbance(int x, int y, float amplitude, float radius) {
    if (radius <= 0.0f) {
        if (x <= 0 || x >= width - 1 || y <= 0 || y >= height - 1) {
            return;
        }
        height_current[idx(x, y)] = amplitude;
        return;
    }

    const int r = static_cast<int>(std::ceil(radius));
    const float sigma = radius * 0.5f;
    const float invTwoSigma2 = 1.0f / (2.0f * sigma * sigma);
    const float r2 = radius * radius;
    for (int dy = -r; dy <= r; ++dy) {
        const int gy = y + dy;
        if (gy <= 0 || gy >= height - 1) {
            continue;
        }
        for (int dxCell = -r; dxCell <= r; ++dxCell) {
            const int gx = x + dxCell;
            if (gx <= 0 || gx >= width - 1) {
                continue;
            }
            const float d2 = static_cast<float>(dxCell * dxCell + dy * dy);
            if (d2 <= r2) {
                height_current[idx(gx, gy)] += amplitude * std::exp(-d2 * invTwoSigma2);
            }
        }
    }
}

void WaveSim::applyWind(float simTime, const WindParams& wind) {
    if (wind.amplitude == 0.0f || !(wind.wavelength > 0.0f)) {
        return;
    }
    const float k = 6.2831853f / wind.wavelength;
    const float envelope = std::cos(simTime * wind.tempo);
    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            height_current[idx(x, y)] +=
                wind.amplitude *
                std::sin(static_cast<float>(x) * k + static_cast<float>(y) * k * 0.37f) *
                envelope;
        }
    }
}
