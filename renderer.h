#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "wave_sim.h"

// OpenGL globals
extern GLFWwindow* window;
extern unsigned int SCR_W, SCR_H;
extern unsigned int causticsFBO, causticsTex;
extern unsigned int waveHeightTex;
extern unsigned int splatProgram, waveVAO, waveVBO;
extern unsigned int displayProgram, screenQuadVAO, screenQuadVBO;
extern unsigned int debugShaderProgram;
extern unsigned int debugVAO, debugVBO;

// Debug shader sources
extern const char* debugVertexShaderSource;
extern const char* debugFragmentShaderSource;

// Function declarations
bool initGL();
bool initDebugShader();
void visualizeRay(const glm::vec3& start, const glm::vec3& end); 