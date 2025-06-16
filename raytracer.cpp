#include <vector>
#include <random>
#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>
#include "wave_sim.h"
#include "raytracer.h"
#include "utils.h"
#include <cmath>

glm::vec3 CausticsRayTracer::generateSunRay() {
    // Generate a random ray within the sun disk
    float theta = dist(rng) * 2.0f * M_PI;
    float r = dist(rng) * sundisk;
    float x = r * cos(theta);
    float y = r * sin(theta);
    return glm::normalize(glm::vec3(x, y, -1.0f));
}

float CausticsRayTracer::calculateLightAttenuation(float distance) {
    // Calculate light attenuation based on water transparency
    return pow(wtrtransp, distance);
}

glm::vec3 CausticsRayTracer::traceRay(const glm::vec3& origin, const glm::vec3& direction, int depth) {
    if (depth >= MAX_DEPTH) return glm::vec3(0.0f);
    
    // Find intersection with water surface
    float t = -origin.z / direction.z;
    if (t < 0) return glm::vec3(0.0f);
    
    glm::vec3 hitPoint = origin + direction * t;
    
    // Get water surface normal at hit point
    int gridX = static_cast<int>(hitPoint.x);
    int gridY = static_cast<int>(hitPoint.y);
    if (gridX < 0 || gridX >= GRID_W-1 || gridY < 0 || gridY >= GRID_H-1) 
        return glm::vec3(0.0f);
        
    // Calculate normal from height field
    glm::vec3 normal(
        height_cur[gridX+1][gridY] - height_cur[gridX-1][gridY],
        height_cur[gridX][gridY+1] - height_cur[gridX][gridY-1],
        2.0f
    );
    normal = glm::normalize(normal);
    
    // Refract ray through water surface
    glm::vec3 refracted = refract(normal, direction);
    
    // Calculate distance to bottom
    float bottomT = (bottomZ - hitPoint.z) / refracted.z;
    if (bottomT < 0) return glm::vec3(0.0f);
    
    glm::vec3 bottomHit = hitPoint + refracted * bottomT;
    
    // Calculate light attenuation
    float attenuation = calculateLightAttenuation(bottomT);
    
    // Calculate caustics intensity based on ray convergence
    float convergence = 1.0f / (1.0f + glm::length(glm::cross(refracted, normal)));
    
    return glm::vec3(attenuation * convergence);
}

CausticsRayTracer::CausticsRayTracer() : 
    rng(std::random_device{}()),
    dist(0.0f, 1.0f),
    sundisk(0.53f),
    wtrtransp(0.77f),
    bottomZ(-20.0f) {}

std::vector<std::vector<glm::vec3>> CausticsRayTracer::renderCaustics(int width, int height) {
    std::vector<std::vector<glm::vec3>> causticsMap(width, std::vector<glm::vec3>(height));
    
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            glm::vec3 totalIntensity(0.0f);
            
            // Monte Carlo sampling
            for (int i = 0; i < NUM_RAYS; i++) {
                glm::vec3 sunRay = generateSunRay();
                glm::vec3 rayOrigin(x, y, 10.0f); // Start rays above water
                totalIntensity += traceRay(rayOrigin, sunRay);
            }
            
            causticsMap[x][y] = totalIntensity / static_cast<float>(NUM_RAYS);
        }
    }
    
    return causticsMap;
} 