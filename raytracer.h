#pragma once

#include <vector>
#include <random>
#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>

// Forward declarations
extern const int GRID_W, GRID_H;
extern std::vector<std::vector<float>> height_cur;

class CausticsRayTracer {
private:
    std::mt19937 rng;
    std::uniform_real_distribution<float> dist;
    
    // Parameters from main.cpp
    const float sundisk;
    const float wtrtransp;
    const float bottomZ;
    
    // Ray tracing parameters
    const int NUM_RAYS = 1000;  // Number of rays per pixel
    const int MAX_DEPTH = 5;    // Maximum ray bounce depth
    
    float calculateLightAttenuation(float distance);
    glm::vec3 traceRay(const glm::vec3& origin, const glm::vec3& direction, int depth = 0);

public:
    CausticsRayTracer();
    std::vector<std::vector<glm::vec3>> renderCaustics(int width, int height);
    glm::vec3 generateSunRay();
}; 