#pragma once

#include <glm/glm.hpp>

inline glm::vec3 refract(glm::vec3 normal, glm::vec3 incident, float ir=1.33f) {
    float n1 = 1.0f;  // air
    float n2 = ir;    // water
    float n1n2 = n1/n2;
    
    float dot = glm::dot(incident, normal);
    float discriminant = 1.0f - n1n2 * n1n2 * (1.0f - dot * dot);
    
    if (discriminant < 0.0f) {
        // Total internal reflection
        return glm::reflect(incident, normal);
    }
    
    return n1n2 * incident - normal * (n1n2 * dot + std::sqrt(discriminant));
} 