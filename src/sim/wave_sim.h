#pragma once

#include <vector>

enum class BoundaryMode {
    ReflectiveNeumann,
    PartialAbsorbRobin,
    AbsorbingOpen,
    FixedDirichlet
};

struct WindParams {
    float amplitude = 0.01f;
    float wavelength = 40.0f;
    float tempo = 0.05f;
};

struct WaveSim {
    int width = 400;
    int height = 400;
    float dx = 1.0f;
    float dt = 0.7f;
    float c = 1.0f;
    float damping = 0.01f;
    BoundaryMode boundaryMode = BoundaryMode::FixedDirichlet;
    float wallReflection = 0.92f;
    std::vector<float> height_current;
    std::vector<float> height_prev;
    std::vector<float> height_next;

    void init(int w = 400, int h = 400);
    void step();
    void addDisturbance(int x, int y, float amplitude, float radius = 0.0f);
    void applyWind(float simTime, const WindParams& wind);
    inline int idx(int x, int y) const { return y * width + x; }
};
