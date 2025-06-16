#include <vector>
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "raytracer.h"
#include "wave_sim.h"
#include "renderer.h"
#include "utils.h"

using namespace std;

int main() {
    if (!initGL()) {
        std::cerr << "Failed to initialize GL" << std::endl;
        return -1;
    }

    if (!initDebugShader()) {
        std::cerr << "Failed to initialize debug shader" << std::endl;
        return -1;
    }

    // Initialize wave simulation with a calm surface
    for (int i = 0; i < GRID_W; i++) {
        for (int j = 0; j < GRID_H; j++) {
            height_cur[i][j] = 0.0f;
        }
    }

    // Create ray tracer
    CausticsRayTracer tracer;
    
    // Test points for ray tracing
    std::vector<glm::vec3> testPoints = {
        glm::vec3(GRID_W/2, GRID_H/2, 10.0f),  // Center
        glm::vec3(GRID_W/4, GRID_H/4, 10.0f),  // Top-left
        glm::vec3(3*GRID_W/4, GRID_H/4, 10.0f), // Top-right
        glm::vec3(GRID_W/4, 3*GRID_H/4, 10.0f), // Bottom-left
        glm::vec3(3*GRID_W/4, 3*GRID_H/4, 10.0f) // Bottom-right
    };

    float time = 0.0f;
    int disturbanceCounter = 0;
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        time += 0.016f; // Assuming 60 FPS
        
        // Create periodic disturbances
        if (disturbanceCounter++ % 60 == 0) { // Every second
            int centerX = rand() % GRID_W;
            int centerY = rand() % GRID_H;
            createWaveDisturbance(centerX, centerY, 0.5f);
        }
        
        // Update wave simulation
        updateWave();
        
        // Clear screen
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Visualize test rays
        glUseProgram(debugShaderProgram);
        for (const auto& startPoint : testPoints) {
            glm::vec3 sunRay = tracer.generateSunRay();
            glm::vec3 hitPoint = startPoint + sunRay * (-startPoint.z / sunRay.z);
            
            // Get surface normal at hit point
            int gridX = static_cast<int>(hitPoint.x);
            int gridY = static_cast<int>(hitPoint.y);
            if (gridX >= 0 && gridX < GRID_W-1 && gridY >= 0 && gridY < GRID_H-1) {
                glm::vec3 normal(
                    height_cur[gridX+1][gridY] - height_cur[gridX-1][gridY],
                    height_cur[gridX][gridY+1] - height_cur[gridX][gridY-1],
                    2.0f
                );
                normal = glm::normalize(normal);
                
                // Calculate refracted ray
                glm::vec3 refracted = refract(normal, sunRay);
                glm::vec3 endPoint = hitPoint + refracted * 20.0f; // Extend ray for visualization
                
                // Draw ray path
                visualizeRay(startPoint, hitPoint);
                visualizeRay(hitPoint, endPoint);
            }
        }
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
