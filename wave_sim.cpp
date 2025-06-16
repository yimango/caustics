#include "wave_sim.h"
#include <cmath>

// Simulation parameters
const int GRID_W = 200, GRID_H = 200;
const float dx = 1.0f;
const float dt = 0.3f;
const float c = 1.0f;
const float damping = 0.0f;    // no damping
const float bottomZ = -20.0f;  // floor depth (negative w.r.t. water surface at z=0)
const float sundisk = 0.53f;   // angle of sun rays
const float wtrtransp = 0.77f; // percent of light not absorbed by unit distance of water

// Wave height fields
std::vector<std::vector<float>> height_cur(GRID_W, std::vector<float>(GRID_H)),
                                height_prev = height_cur,
                                height_next = height_cur;

void updateWave() {
    for(int i=1; i<GRID_W-1; i++) {
        for(int j=1; j<GRID_H-1; j++) {
            float lap = height_cur[i+1][j]
                      + height_cur[i-1][j]
                      + height_cur[i][j+1]
                      + height_cur[i][j-1]
                      - 4*height_cur[i][j];
            height_next[i][j] = (1-damping)*(2*height_cur[i][j] - height_prev[i][j])
                              + (c*c*dt*dt/(dx*dx))*lap;
        }
    }
    height_prev = height_cur;
    height_cur  = height_next;
}

void createWaveDisturbance(int centerX, int centerY, float amplitude) {
    for (int i = 0; i < GRID_W; i++) {
        for (int j = 0; j < GRID_H; j++) {
            float dx = i - centerX;
            float dy = j - centerY;
            float dist = sqrt(dx*dx + dy*dy);
            height_cur[i][j] += amplitude * exp(-dist*dist/100.0f);
        }
    }
} 