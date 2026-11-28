#include "sim/wave_sim.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kMaxCourant = 0.65f;
constexpr float kMaxSpongeDamp = 0.16f;
constexpr int kMaxSpongeWidth = 12;

int mirrorIndex(int v, int size) {
    if (size <= 1) {
        return 0;
    }
    const int maxIdx = size - 1;
    while (v < 0 || v > maxIdx) {
        if (v < 0) {
            v = -v;
        }
        if (v > maxIdx) {
            v = 2 * maxIdx - v;
        }
    }
    return v;
}

float smoothstep01(float t) {
    const float u = std::clamp(t, 0.0f, 1.0f);
    return u * u * (3.0f - 2.0f * u);
}

} // namespace

void WaveSim::init(int w, int h) {
    width = std::max(0, w);
    height = std::max(0, h);
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    height_current.assign(count, 0.0f);
    height_prev.assign(count, 0.0f);
    height_next.assign(count, 0.0f);
}

void WaveSim::reset() {
    const std::size_t count = static_cast<std::size_t>(std::max(0, width)) * static_cast<std::size_t>(std::max(0, height));
    if (height_current.size() != count || height_prev.size() != count || height_next.size() != count) {
        init(width, height);
        return;
    }
    std::fill(height_current.begin(), height_current.end(), 0.0f);
    std::fill(height_prev.begin(), height_prev.end(), 0.0f);
    std::fill(height_next.begin(), height_next.end(), 0.0f);
}

void WaveSim::stepFixed(float deltaSeconds, float& accumulator, int maxSubsteps, float fixedStepSeconds) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f ||
        !std::isfinite(fixedStepSeconds) || fixedStepSeconds <= 0.0f ||
        maxSubsteps <= 0) {
        return;
    }
    if (!std::isfinite(accumulator) || accumulator < 0.0f) {
        accumulator = 0.0f;
    }

    const float clampedDelta = std::min(deltaSeconds, 0.25f);
    accumulator += clampedDelta;

    int steps = 0;
    while (accumulator >= fixedStepSeconds && steps < maxSubsteps) {
        step();
        accumulator -= fixedStepSeconds;
        ++steps;
    }

    if (steps == maxSubsteps && accumulator >= fixedStepSeconds) {
        accumulator = std::fmod(accumulator, fixedStepSeconds);
    }
}

void WaveSim::step() {
    if (width <= 0 || height <= 0) {
        return;
    }
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (height_current.size() < count || height_prev.size() < count || height_next.size() < count) {
        return;
    }

    const float safeDx = (dx > 1e-6f) ? dx : 1.0f;
    const float rawR = (c * dt) / safeDx;
    const float r = std::clamp(rawR, 0.0f, kMaxCourant);
    const float r2 = r * r;
    const float d = std::clamp(damping, 0.0f, 1.0f);
    const float velDamp = 1.0f - d;

    const float* curBuf = height_current.data();
    const float* prevBuf = height_prev.data();
    float* nextBuf = height_next.data();

    if (width == 1 || height == 1) {
        if (boundaryMode == BoundaryMode::FixedDirichlet) {
            std::fill_n(nextBuf, count, 0.0f);
        } else {
            for (std::size_t i = 0; i < count; ++i) {
                nextBuf[i] = curBuf[i] + velDamp * (curBuf[i] - prevBuf[i]);
            }
        }
        height_prev.swap(height_current);
        height_current.swap(height_next);
        return;
    }

    const int W = width;
    const int H = height;

    // 1. Interior update (all modes; AbsorbingOpen adds a smooth cosine-tapered sponge layer).
    if (boundaryMode == BoundaryMode::AbsorbingOpen) {
        const int spongeWidth = std::min(kMaxSpongeWidth, std::min(W, H) / 4);
        for (int y = 1; y < H - 1; ++y) {
            const int distY = std::min(y, H - 1 - y);
            const int row = y * W;
            for (int x = 1; x < W - 1; ++x) {
                const int i = row + x;
                const float cur = curBuf[i];
                const float prev = prevBuf[i];
                const float laplacian =
                    ((curBuf[i + 1] + curBuf[i - 1]) +
                     (curBuf[i + W] + curBuf[i - W])) -
                    4.0f * cur;

                float localVelDamp = velDamp;
                if (spongeWidth > 1) {
                    const int dist = std::min(std::min(x, W - 1 - x), distY);
                    if (dist < spongeWidth) {
                        const float s = static_cast<float>(spongeWidth - dist) / static_cast<float>(spongeWidth);
                        const float taper = 0.5f * (1.0f - std::cos(kPi * s));
                        const float localDamp = std::min(1.0f, d + kMaxSpongeDamp * taper);
                        localVelDamp = 1.0f - localDamp;
                    }
                }
                nextBuf[i] = cur + localVelDamp * (cur - prev) + r2 * laplacian;
            }
        }
    } else {
        for (int y = 1; y < H - 1; ++y) {
            const int row = y * W;
            for (int x = 1; x < W - 1; ++x) {
                const int i = row + x;
                const float cur = curBuf[i];
                const float prev = prevBuf[i];
                const float laplacian =
                    ((curBuf[i + 1] + curBuf[i - 1]) +
                     (curBuf[i + W] + curBuf[i - W])) -
                    4.0f * cur;
                nextBuf[i] = cur + velDamp * (cur - prev) + r2 * laplacian;
            }
        }
    }

    // 2. Boundary & corner updates according to BoundaryMode.
    switch (boundaryMode) {
    case BoundaryMode::ReflectiveNeumann: {
        // Bottom edge (y = 0) and Top edge (y = H - 1)
        const int topRow = (H - 1) * W;
        const int topInRow = (H - 2) * W;
        for (int x = 1; x < W - 1; ++x) {
            {
                const int i = x;
                const float cur = curBuf[i];
                const float lap = (2.0f * curBuf[W + x] + (curBuf[i - 1] + curBuf[i + 1])) - 4.0f * cur;
                nextBuf[i] = cur + velDamp * (cur - prevBuf[i]) + r2 * lap;
            }
            {
                const int i = topRow + x;
                const float cur = curBuf[i];
                const float lap = (2.0f * curBuf[topInRow + x] + (curBuf[i - 1] + curBuf[i + 1])) - 4.0f * cur;
                nextBuf[i] = cur + velDamp * (cur - prevBuf[i]) + r2 * lap;
            }
        }

        // Left edge (x = 0) and Right edge (x = W - 1)
        for (int y = 1; y < H - 1; ++y) {
            const int row = y * W;
            {
                const int i = row;
                const float cur = curBuf[i];
                const float lap = (2.0f * curBuf[i + 1] + (curBuf[i - W] + curBuf[i + W])) - 4.0f * cur;
                nextBuf[i] = cur + velDamp * (cur - prevBuf[i]) + r2 * lap;
            }
            {
                const int i = row + (W - 1);
                const float cur = curBuf[i];
                const float lap = (2.0f * curBuf[i - 1] + (curBuf[i - W] + curBuf[i + W])) - 4.0f * cur;
                nextBuf[i] = cur + velDamp * (cur - prevBuf[i]) + r2 * lap;
            }
        }

        // 4 Corners (2nd-order symmetric ghost cells on both axes)
        {
            const int i = 0;
            const float cur = curBuf[i];
            const float lap = (2.0f * curBuf[1] + 2.0f * curBuf[W]) - 4.0f * cur;
            nextBuf[i] = cur + velDamp * (cur - prevBuf[i]) + r2 * lap;
        }
        {
            const int i = W - 1;
            const float cur = curBuf[i];
            const float lap = (2.0f * curBuf[W - 2] + 2.0f * curBuf[W + (W - 1)]) - 4.0f * cur;
            nextBuf[i] = cur + velDamp * (cur - prevBuf[i]) + r2 * lap;
        }
        {
            const int i = topRow;
            const float cur = curBuf[i];
            const float lap = (2.0f * curBuf[topRow + 1] + 2.0f * curBuf[topInRow]) - 4.0f * cur;
            nextBuf[i] = cur + velDamp * (cur - prevBuf[i]) + r2 * lap;
        }
        {
            const int i = topRow + (W - 1);
            const float cur = curBuf[i];
            const float lap = (2.0f * curBuf[topRow + (W - 2)] + 2.0f * curBuf[topInRow + (W - 1)]) - 4.0f * cur;
            nextBuf[i] = cur + velDamp * (cur - prevBuf[i]) + r2 * lap;
        }
        break;
    }

    case BoundaryMode::PartialAbsorbRobin: {
        // Exact discrete impedance Robin BC: dH/dn + (alpha / c) * dH/dt = 0
        // with alpha = (1 - R) / (1 + R) so normal wave reflection equals R = wallReflection.
        const float refl = std::clamp(wallReflection, 0.0f, 1.0f);
        const float alpha = (1.0f - refl) / (1.0f + refl);
        const float betaEdge = r * alpha;
        const float edgeInv = 1.0f / (1.0f + betaEdge);
        const float edgeVelCoeff = (velDamp - betaEdge) * edgeInv;
        const float edgeLapCoeff = r2 * edgeInv;

        const float betaCorner = 2.0f * r * alpha;
        const float cornerInv = 1.0f / (1.0f + betaCorner);
        const float cornerVelCoeff = (velDamp - betaCorner) * cornerInv;
        const float cornerLapCoeff = r2 * cornerInv;

        const int topRow = (H - 1) * W;
        const int topInRow = (H - 2) * W;
        for (int x = 1; x < W - 1; ++x) {
            {
                const int i = x;
                const float cur = curBuf[i];
                const float lap = (2.0f * curBuf[W + x] + (curBuf[i - 1] + curBuf[i + 1])) - 4.0f * cur;
                nextBuf[i] = cur + edgeVelCoeff * (cur - prevBuf[i]) + edgeLapCoeff * lap;
            }
            {
                const int i = topRow + x;
                const float cur = curBuf[i];
                const float lap = (2.0f * curBuf[topInRow + x] + (curBuf[i - 1] + curBuf[i + 1])) - 4.0f * cur;
                nextBuf[i] = cur + edgeVelCoeff * (cur - prevBuf[i]) + edgeLapCoeff * lap;
            }
        }

        for (int y = 1; y < H - 1; ++y) {
            const int row = y * W;
            {
                const int i = row;
                const float cur = curBuf[i];
                const float lap = (2.0f * curBuf[i + 1] + (curBuf[i - W] + curBuf[i + W])) - 4.0f * cur;
                nextBuf[i] = cur + edgeVelCoeff * (cur - prevBuf[i]) + edgeLapCoeff * lap;
            }
            {
                const int i = row + (W - 1);
                const float cur = curBuf[i];
                const float lap = (2.0f * curBuf[i - 1] + (curBuf[i - W] + curBuf[i + W])) - 4.0f * cur;
                nextBuf[i] = cur + edgeVelCoeff * (cur - prevBuf[i]) + edgeLapCoeff * lap;
            }
        }

        {
            const int i = 0;
            const float cur = curBuf[i];
            const float lap = (2.0f * curBuf[1] + 2.0f * curBuf[W]) - 4.0f * cur;
            nextBuf[i] = cur + cornerVelCoeff * (cur - prevBuf[i]) + cornerLapCoeff * lap;
        }
        {
            const int i = W - 1;
            const float cur = curBuf[i];
            const float lap = (2.0f * curBuf[W - 2] + 2.0f * curBuf[W + (W - 1)]) - 4.0f * cur;
            nextBuf[i] = cur + cornerVelCoeff * (cur - prevBuf[i]) + cornerLapCoeff * lap;
        }
        {
            const int i = topRow;
            const float cur = curBuf[i];
            const float lap = (2.0f * curBuf[topRow + 1] + 2.0f * curBuf[topInRow]) - 4.0f * cur;
            nextBuf[i] = cur + cornerVelCoeff * (cur - prevBuf[i]) + cornerLapCoeff * lap;
        }
        {
            const int i = topRow + (W - 1);
            const float cur = curBuf[i];
            const float lap = (2.0f * curBuf[topRow + (W - 2)] + 2.0f * curBuf[topInRow + (W - 1)]) - 4.0f * cur;
            nextBuf[i] = cur + cornerVelCoeff * (cur - prevBuf[i]) + cornerLapCoeff * lap;
        }
        break;
    }

    case BoundaryMode::AbsorbingOpen: {
        const int topRow = (H - 1) * W;
        const int topInRow = (H - 2) * W;

        if (W < 3 || H < 3) {
            // Degenerate 2x2 grid: fall back to fully matched Robin impedance (alpha = 1).
            const float betaCorner = 2.0f * r;
            const float cornerInv = 1.0f / (1.0f + betaCorner);
            const float cornerVelCoeff = (velDamp - betaCorner) * cornerInv;
            const float cornerLapCoeff = r2 * cornerInv;
            const int corners[4][3] = {
                {0, 1, W},
                {W - 1, W - 2, W + (W - 1)},
                {topRow, topRow + 1, topInRow},
                {topRow + (W - 1), topRow + (W - 2), topInRow + (W - 1)}
            };
            for (const auto& cIdx : corners) {
                const int i = cIdx[0];
                const float cur = curBuf[i];
                const float lap = (2.0f * curBuf[cIdx[1]] + 2.0f * curBuf[cIdx[2]]) - 4.0f * cur;
                nextBuf[i] = cur + cornerVelCoeff * (cur - prevBuf[i]) + cornerLapCoeff * lap;
            }
            break;
        }

        // 1st-order Mur absorbing boundary condition on all 4 edges.
        const float mur = (r - 1.0f) / (r + 1.0f);

        for (int x = 1; x < W - 1; ++x) {
            {
                const int edge = x;
                const int inner = W + x;
                nextBuf[edge] = curBuf[inner] + mur * (nextBuf[inner] - curBuf[edge]);
            }
            {
                const int edge = topRow + x;
                const int inner = topInRow + x;
                nextBuf[edge] = curBuf[inner] + mur * (nextBuf[inner] - curBuf[edge]);
            }
        }

        for (int y = 1; y < H - 1; ++y) {
            const int row = y * W;
            {
                const int edge = row;
                const int inner = row + 1;
                nextBuf[edge] = curBuf[inner] + mur * (nextBuf[inner] - curBuf[edge]);
            }
            {
                const int edge = row + (W - 1);
                const int inner = row + (W - 2);
                nextBuf[edge] = curBuf[inner] + mur * (nextBuf[inner] - curBuf[edge]);
            }
        }

        // Smooth corner averaging: (mur_x + mur_y) * 0.5
        {
            const int c0 = 0;
            const float murX = curBuf[1] + mur * (nextBuf[1] - curBuf[c0]);
            const float murY = curBuf[W] + mur * (nextBuf[W] - curBuf[c0]);
            nextBuf[c0] = 0.5f * (murX + murY);
        }
        {
            const int c1 = W - 1;
            const float murX = curBuf[W - 2] + mur * (nextBuf[W - 2] - curBuf[c1]);
            const float murY = curBuf[W + (W - 1)] + mur * (nextBuf[W + (W - 1)] - curBuf[c1]);
            nextBuf[c1] = 0.5f * (murX + murY);
        }
        {
            const int c2 = topRow;
            const float murX = curBuf[topRow + 1] + mur * (nextBuf[topRow + 1] - curBuf[c2]);
            const float murY = curBuf[topInRow] + mur * (nextBuf[topInRow] - curBuf[c2]);
            nextBuf[c2] = 0.5f * (murX + murY);
        }
        {
            const int c3 = topRow + (W - 1);
            const float murX = curBuf[topRow + (W - 2)] + mur * (nextBuf[topRow + (W - 2)] - curBuf[c3]);
            const float murY = curBuf[topInRow + (W - 1)] + mur * (nextBuf[topInRow + (W - 1)] - curBuf[c3]);
            nextBuf[c3] = 0.5f * (murX + murY);
        }
        break;
    }

    case BoundaryMode::FixedDirichlet: {
        const int topRow = (H - 1) * W;
        for (int x = 0; x < W; ++x) {
            nextBuf[x] = 0.0f;
            nextBuf[topRow + x] = 0.0f;
        }
        for (int y = 1; y < H - 1; ++y) {
            const int row = y * W;
            nextBuf[row] = 0.0f;
            nextBuf[row + (W - 1)] = 0.0f;
        }
        break;
    }
    }

    // O(1) 3-buffer rotation.
    height_prev.swap(height_current);
    height_current.swap(height_next);
}

void WaveSim::addDisturbance(int x, int y, float amplitude, float radius) {
    if (width <= 0 || height <= 0 || !std::isfinite(amplitude) || amplitude == 0.0f) {
        return;
    }
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (height_current.size() < count || height_prev.size() < count) {
        return;
    }

    const bool mirrorWalls =
        (boundaryMode == BoundaryMode::ReflectiveNeumann ||
         boundaryMode == BoundaryMode::PartialAbsorbRobin);

    if (radius <= 0.5f) {
        if (boundaryMode == BoundaryMode::FixedDirichlet) {
            if (x <= 0 || x >= width - 1 || y <= 0 || y >= height - 1) {
                return;
            }
        } else {
            if (x < 0 || x >= width || y < 0 || y >= height) {
                return;
            }
        }
        const int i = idx(x, y);
        height_current[i] += amplitude;
        height_prev[i] += amplitude;
        return;
    }

    const int r = static_cast<int>(std::ceil(radius));
    const float r2 = radius * radius;
    const float invRadius = 1.0f / radius;

    for (int dy = -r; dy <= r; ++dy) {
        for (int dxCell = -r; dxCell <= r; ++dxCell) {
            const float d2 = static_cast<float>(dxCell * dxCell + dy * dy);
            if (d2 >= r2) {
                continue;
            }

            int gx = x + dxCell;
            int gy = y + dy;
            if (mirrorWalls) {
                gx = mirrorIndex(gx, width);
                gy = mirrorIndex(gy, height);
            } else if (boundaryMode == BoundaryMode::FixedDirichlet) {
                if (gx <= 0 || gx >= width - 1 || gy <= 0 || gy >= height - 1) {
                    continue;
                }
            } else {
                if (gx < 0 || gx >= width || gy < 0 || gy >= height) {
                    continue;
                }
            }

            // C1-smooth raised-cosine splash profile (1 at center, 0 at radius, zero slope at both ends).
            const float d = std::sqrt(d2);
            const float weight = 0.5f * (1.0f + std::cos(kPi * d * invRadius));
            float delta = amplitude * weight;
            if (mirrorWalls) {
                // Edge (weight 0.5) and corner (weight 0.25) cells sit on mirror planes and have no
                // separate reflected twin under mirrorIndex; dividing by their trapezoidal weight
                // reproduces the exact method-of-images superposition (dH/dn = 0 and exact volume).
                const float wx = (width > 1 && (gx == 0 || gx == width - 1)) ? 0.5f : 1.0f;
                const float wy = (height > 1 && (gy == 0 || gy == height - 1)) ? 0.5f : 1.0f;
                delta /= (wx * wy);
            }
            const int i = idx(gx, gy);
            height_current[i] += delta;
            height_prev[i] += delta;
        }
    }
}

void WaveSim::applyWind(float simTime, const WindParams& wind) {
    if (width <= 0 || height <= 0 || !std::isfinite(wind.amplitude) || wind.amplitude == 0.0f ||
        !(wind.wavelength > 0.0f)) {
        return;
    }
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (height_current.size() < count) {
        return;
    }

    // 4 directional traveling wave components with incommensurate headings and wavelengths.
    // Wavenumber and speed scales avoid low-order resonant amplification at default WindParams.
    struct WaveComponent {
        float dirX;
        float dirY;
        float kScale;
        float speedScale;
        float weight;
        float phase;
    };
    static constexpr WaveComponent kComponents[4] = {
        { 0.9611f,  0.2764f, 1.00f, 0.72f, 0.38f, 0.00f},
        { 0.3809f,  0.9246f, 1.27f, 0.91f, 0.27f, 1.37f},
        {-0.6736f,  0.7391f, 1.53f, 1.07f, 0.20f, 2.83f},
        { 0.8139f, -0.5810f, 1.81f, 0.58f, 0.15f, 4.19f}
    };

    const float safeDx = (dx > 1e-6f) ? dx : 1.0f;
    const float rScale = std::clamp((c * dt) / safeDx, 0.0f, kMaxCourant);
    const float baseAmp = wind.amplitude * rScale * 0.4f;

    const float baseK = kTwoPi / wind.wavelength;
    float kx[4];
    float ky[4];
    float phaseT[4];
    float meanOffset[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    for (int m = 0; m < 4; ++m) {
        const float km = baseK * kComponents[m].kScale;
        kx[m] = km * kComponents[m].dirX;
        ky[m] = km * kComponents[m].dirY;
        phaseT[m] = kComponents[m].phase - wind.tempo * kComponents[m].speedScale * simTime;
    }

    const int W = width;
    const int H = height;

    // For non-Dirichlet modes, subtract each component's exact trapezoidal grid mean in O(W + H)
    // so continuous wind forcing never introduces net volume drift.
    if (boundaryMode != BoundaryMode::FixedDirichlet && W > 1 && H > 1) {
        const double invSpanX = 1.0 / static_cast<double>(W - 1);
        const double invSpanY = 1.0 / static_cast<double>(H - 1);
        for (int m = 0; m < 4; ++m) {
            double cx = 0.0;
            double sx = 0.0;
            for (int x = 0; x < W; ++x) {
                const double wx = (x == 0 || x == W - 1) ? 0.5 : 1.0;
                const double arg = static_cast<double>(kx[m]) * static_cast<double>(x) + static_cast<double>(phaseT[m]);
                cx += wx * std::cos(arg);
                sx += wx * std::sin(arg);
            }
            double cy = 0.0;
            double sy = 0.0;
            for (int y = 0; y < H; ++y) {
                const double wy = (y == 0 || y == H - 1) ? 0.5 : 1.0;
                const double arg = static_cast<double>(ky[m]) * static_cast<double>(y);
                cy += wy * std::cos(arg);
                sy += wy * std::sin(arg);
            }
            meanOffset[m] = static_cast<float>((sx * cy + cx * sy) * invSpanX * invSpanY);
        }
    }

    const bool dirichlet = (boundaryMode == BoundaryMode::FixedDirichlet);
    const float taperWidth = static_cast<float>(std::max(1, std::min(8, std::min(W, H) / 4)));
    const float invTaperWidth = 1.0f / taperWidth;
    float* curBuf = height_current.data();

    const int yStart = dirichlet ? 1 : 0;
    const int yEnd = dirichlet ? (H - 1) : H;
    const int xStart = dirichlet ? 1 : 0;
    const int xEnd = dirichlet ? (W - 1) : W;

    for (int y = yStart; y < yEnd; ++y) {
        const float fy = static_cast<float>(y);
        float ty = 1.0f;
        if (dirichlet) {
            const int distY = std::min(y, H - 1 - y);
            if (distY < static_cast<int>(taperWidth)) {
                ty = smoothstep01(static_cast<float>(distY) * invTaperWidth);
            }
        }
        const float yPhase0 = ky[0] * fy + phaseT[0];
        const float yPhase1 = ky[1] * fy + phaseT[1];
        const float yPhase2 = ky[2] * fy + phaseT[2];
        const float yPhase3 = ky[3] * fy + phaseT[3];
        const int row = y * W;

        for (int x = xStart; x < xEnd; ++x) {
            const float fx = static_cast<float>(x);
            float envelope = baseAmp * ty;
            if (dirichlet) {
                const int distX = std::min(x, W - 1 - x);
                if (distX < static_cast<int>(taperWidth)) {
                    envelope *= smoothstep01(static_cast<float>(distX) * invTaperWidth);
                }
            }
            const float wave =
                kComponents[0].weight * (std::sin(kx[0] * fx + yPhase0) - meanOffset[0]) +
                kComponents[1].weight * (std::sin(kx[1] * fx + yPhase1) - meanOffset[1]) +
                kComponents[2].weight * (std::sin(kx[2] * fx + yPhase2) - meanOffset[2]) +
                kComponents[3].weight * (std::sin(kx[3] * fx + yPhase3) - meanOffset[3]);
            curBuf[row + x] += envelope * wave;
        }
    }
}

float WaveSim::totalVolume() const {
    if (width <= 0 || height <= 0) {
        return 0.0f;
    }
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (height_current.size() < count) {
        return 0.0f;
    }

    const int W = width;
    const int H = height;
    const float* curBuf = height_current.data();
    double sum = 0.0;

    for (int y = 0; y < H; ++y) {
        const double wy = (H > 1 && (y == 0 || y == H - 1)) ? 0.5 : 1.0;
        const int row = y * W;
        double rowSum = 0.0;
        for (int x = 0; x < W; ++x) {
            const double wx = (W > 1 && (x == 0 || x == W - 1)) ? 0.5 : 1.0;
            rowSum += wx * static_cast<double>(curBuf[row + x]);
        }
        sum += wy * rowSum;
    }

    const double cellArea = static_cast<double>(dx) * static_cast<double>(dx);
    return static_cast<float>(sum * cellArea);
}

float WaveSim::totalEnergy() const {
    if (width <= 0 || height <= 0) {
        return 0.0f;
    }
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (height_current.size() < count || height_prev.size() < count) {
        return 0.0f;
    }

    const int W = width;
    const int H = height;
    const double safeDx = (dx > 1e-6f) ? static_cast<double>(dx) : 1.0;
    const double safeDt = (dt > 1e-6f) ? static_cast<double>(dt) : 1.0;
    const double invDt = 1.0 / safeDt;
    const double cellArea = safeDx * safeDx;
    const double c2 = static_cast<double>(c) * static_cast<double>(c);
    const float* curBuf = height_current.data();
    const float* prevBuf = height_prev.data();

    double kinetic = 0.0;
    for (int y = 0; y < H; ++y) {
        const double wy = (H > 1 && (y == 0 || y == H - 1)) ? 0.5 : 1.0;
        const int row = y * W;
        double rowKe = 0.0;
        for (int x = 0; x < W; ++x) {
            const double wx = (W > 1 && (x == 0 || x == W - 1)) ? 0.5 : 1.0;
            const double vel = (static_cast<double>(curBuf[row + x]) - static_cast<double>(prevBuf[row + x])) * invDt;
            rowKe += wx * vel * vel;
        }
        kinetic += wy * rowKe;
    }
    kinetic *= 0.5 * cellArea;

    double potential = 0.0;
    if (W > 1) {
        for (int y = 0; y < H; ++y) {
            const double wy = (H > 1 && (y == 0 || y == H - 1)) ? 0.5 : 1.0;
            const int row = y * W;
            double rowPe = 0.0;
            for (int x = 0; x < W - 1; ++x) {
                const double diff = static_cast<double>(curBuf[row + x + 1]) - static_cast<double>(curBuf[row + x]);
                rowPe += diff * diff;
            }
            potential += wy * rowPe;
        }
    }
    if (H > 1) {
        for (int y = 0; y < H - 1; ++y) {
            const int row = y * W;
            const int nextRow = (y + 1) * W;
            double rowPe = 0.0;
            for (int x = 0; x < W; ++x) {
                const double wx = (W > 1 && (x == 0 || x == W - 1)) ? 0.5 : 1.0;
                const double diff = static_cast<double>(curBuf[nextRow + x]) - static_cast<double>(curBuf[row + x]);
                rowPe += wx * diff * diff;
            }
            potential += rowPe;
        }
    }
    potential *= 0.5 * c2;

    return static_cast<float>(kinetic + potential);
}
