#pragma once

#include <vector>
#include <glm/glm.hpp>

// Simulation parameters
extern const int GRID_W, GRID_H;
extern const float dx, dt, c, damping, bottomZ, sundisk, wtrtransp;

// Wave height fields
extern std::vector<std::vector<float>> height_cur;
extern std::vector<std::vector<float>> height_prev;
extern std::vector<std::vector<float>> height_next;

// Wave simulation functions
void updateWave();
void createWaveDisturbance(int centerX, int centerY, float amplitude); 