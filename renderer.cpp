#include "renderer.h"
#include <iostream>

// OpenGL globals
GLFWwindow* window;
unsigned int SCR_W = 800, SCR_H = 600;
unsigned int causticsFBO, causticsTex;
unsigned int waveHeightTex;
unsigned int splatProgram, waveVAO, waveVBO;
unsigned int displayProgram, screenQuadVAO, screenQuadVBO;
unsigned int debugShaderProgram;
unsigned int debugVAO, debugVBO;

// Debug shader sources
const char* debugVertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec2 aPos;
    void main() {
        gl_Position = vec4(aPos, 0.0, 1.0);
    }
)";

const char* debugFragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    void main() {
        FragColor = vec4(1.0, 1.0, 0.0, 1.0); // Yellow for rays
    }
)";

bool initGL() {
    if(!glfwInit()) return false;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(SCR_W,SCR_H,"Caustics Only",nullptr,nullptr);
    if(!window){ glfwTerminate(); return false; }
    glfwMakeContextCurrent(window);
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        return false;
    // prepare FBO blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE,GL_ONE);
    
    // Setup screen quad
    float quadVertices[] = {
        -1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f
    };
    glGenVertexArrays(1, &screenQuadVAO);
    glGenBuffers(1, &screenQuadVBO);
    glBindVertexArray(screenQuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, screenQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    return true;
}

bool initDebugShader() {
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &debugVertexShaderSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &debugFragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    debugShaderProgram = glCreateProgram();
    glAttachShader(debugShaderProgram, vertexShader);
    glAttachShader(debugShaderProgram, fragmentShader);
    glLinkProgram(debugShaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Setup debug VAO for ray visualization
    glGenVertexArrays(1, &debugVAO);
    glGenBuffers(1, &debugVBO);
    glBindVertexArray(debugVAO);
    glBindBuffer(GL_ARRAY_BUFFER, debugVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    return true;
}

void visualizeRay(const glm::vec3& start, const glm::vec3& end) {
    float vertices[] = {
        start.x/GRID_W * 2.0f - 1.0f, start.y/GRID_H * 2.0f - 1.0f,
        end.x/GRID_W * 2.0f - 1.0f, end.y/GRID_H * 2.0f - 1.0f
    };
    
    glBindVertexArray(debugVAO);
    glBindBuffer(GL_ARRAY_BUFFER, debugVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_LINES, 0, 2);
} 