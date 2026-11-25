#include <vector>
#include <iostream>
#include <chrono>
#include <cmath>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shaders.h"

using namespace std;

// Water simulation parameters
int width = 400;
int height = 400;
float dx = 1.0f;        // Grid spacing
float dt = 0.7f;        // Time step
float c = 1.0f;         // Wave speed
float damping = 0.01f;  // Damping factor
float waterScale = 1.0f; // World size stays 400 across (was 200 * 2)

// Water height grids for simulation
std::vector<std::vector<float>> height_current(width, std::vector<float>(height));
std::vector<std::vector<float>> height_prev(width, std::vector<float>(height));
std::vector<std::vector<float>> height_next(width, std::vector<float>(height));

// Physical constants
const float WATER_IOR = 1.33f;  // Index of refraction for water
const float AIR_IOR = 1.0f;     // Index of refraction for air

// OpenGL window dimensions
const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 900;
const int CAUSTICS_RES = 2048;

// Function prototypes
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource);
void setupCausticsFBO();

// Global variables
GLFWwindow* window;
unsigned int waterVAO, waterVBO, waterEBO;
unsigned int bottomVAO, bottomVBO, bottomEBO;
unsigned int skyboxVAO, skyboxVBO;
unsigned int causticsFBO, causticsTexture;
unsigned int waterShaderProgram;
unsigned int skyboxShaderProgram;
unsigned int causticsShaderProgram;
unsigned int bottomShaderProgram;

const float BOTTOM_Z = -30.0f; // Bottom surface Z coordinate (deeper for larger scale)

std::vector<float> waterVertices;
std::vector<unsigned int> waterIndices;

void update_wave(){
    for (int i = 1; i < width - 1; ++i){
        for (int j = 1; j < height - 1; ++j){
            float laplacian =
                height_current[i + 1][j] +
                height_current[i - 1][j] +
                height_current[i][j + 1] +
                height_current[i][j - 1] -
                4 * height_current[i][j];

            // Update using the wave equation
            height_next[i][j] = (1 - damping) * (2 * height_current[i][j] - height_prev[i][j]) +
                                (c * c * dt * dt / (dx * dx)) * laplacian;
        }
    }

    // Copy, not swap. The next buffer keeps a zero border; a rotate would keep stale border cells.
    height_prev = height_current;
    height_current = height_next;
}

void add_disturbance(int x, int y, float amplitude){
    if (x <= 0 || x >= width - 1 || y <= 0 || y >= height - 1) {
        return;
    }
    height_current[x][y] = amplitude;
}

void init_grid(){
    for(int i = 0; i < width; i++){
        for(int j = 0; j < height; j++){
            height_current[i][j] = 0.0f;
            height_prev[i][j] = 0.0f;
            height_next[i][j] = 0.0f;
        }
    }
}

glm::vec3 getSurfaceNormal(int x, int y) {
    float ddx = (height_current[x+1][y] - height_current[x-1][y]) / (2.0f * dx) * 8.0f;
    float ddy = (height_current[x][y+1] - height_current[x][y-1]) / (2.0f * dx) * 8.0f;
    glm::vec3 n(-ddx, -ddy, 1.0f);
    return glm::normalize(n);
}

// Initialize OpenGL
bool initGL() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Water Caustics", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback); // Mouse click callback

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    return true;
}

// Generate water surface mesh
void generateWaterMesh() {
    waterVertices.clear();
    waterIndices.clear();
    
    // Generate vertices
    for (int i = 0; i < width; i++) {
        for (int j = 0; j < height; j++) {
            // Position (scaled to fill more of the viewport)
            waterVertices.push_back((i - width/2.0f) * waterScale);
            waterVertices.push_back((j - height/2.0f) * waterScale);
            waterVertices.push_back(height_current[i][j] * 8.0f);
            
            // Compute normal using getSurfaceNormal
            glm::vec3 normal;
            if (i > 0 && i < width-1 && j > 0 && j < height-1) {
                normal = getSurfaceNormal(i, j);
            } else {
                normal = glm::vec3(0.0f, 0.0f, 1.0f); // Default normal for edges
            }
            
            // Normal
            waterVertices.push_back(normal.x);
            waterVertices.push_back(normal.y);
            waterVertices.push_back(normal.z);
        }
    }
    
    // Generate indices
    for (int i = 0; i < width-1; i++) {
        for (int j = 0; j < height-1; j++) {
            unsigned int topLeft = i * height + j;
            unsigned int topRight = topLeft + 1;
            unsigned int bottomLeft = (i + 1) * height + j;
            unsigned int bottomRight = bottomLeft + 1;
            
            waterIndices.push_back(topLeft);
            waterIndices.push_back(bottomLeft);
            waterIndices.push_back(topRight);
            
            waterIndices.push_back(topRight);
            waterIndices.push_back(bottomLeft);
            waterIndices.push_back(bottomRight);
        }
    }
}

// Generate bottom surface mesh
void generateBottomMesh() {
    float bottom_y = BOTTOM_Z; // Use the constant for bottom Z coordinate
    float half_width = (width / 2.0f) * waterScale;
    float half_height = (height / 2.0f) * waterScale;

    // Vertices for a simple quad
    float bottomVertices[] = {
        // positions            // normals
        -half_width, -half_height, bottom_y,  0.0f, 0.0f, 1.0f,
         half_width, -half_height, bottom_y,  0.0f, 0.0f, 1.0f,
         half_width,  half_height, bottom_y,  0.0f, 0.0f, 1.0f,
        -half_width,  half_height, bottom_y,  0.0f, 0.0f, 1.0f
    };

    unsigned int bottomIndices[] = {
        0, 1, 2,
        2, 3, 0
    };

    glGenVertexArrays(1, &bottomVAO);
    glGenBuffers(1, &bottomVBO);
    glGenBuffers(1, &bottomEBO);

    glBindVertexArray(bottomVAO);

    glBindBuffer(GL_ARRAY_BUFFER, bottomVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(bottomVertices), bottomVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bottomEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(bottomIndices), bottomIndices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

// Setup OpenGL buffers for water
void setupWaterBuffers() {
    glGenVertexArrays(1, &waterVAO);
    glGenBuffers(1, &waterVBO);
    glGenBuffers(1, &waterEBO);
    
    glBindVertexArray(waterVAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
    glBufferData(GL_ARRAY_BUFFER, waterVertices.size() * sizeof(float), waterVertices.data(), GL_DYNAMIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, waterEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, waterIndices.size() * sizeof(unsigned int), waterIndices.data(), GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

// Generate skybox mesh
void generateSkyboxMesh() {
    float skyboxVertices[] = {
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f
    };

    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
}

// Add after other function prototypes
void setupCausticsFBO() {
    // Create FBO
    glGenFramebuffers(1, &causticsFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, causticsFBO);
    
    // Create texture
    glGenTextures(1, &causticsTexture);
    glBindTexture(GL_TEXTURE_2D, causticsTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, CAUSTICS_RES, CAUSTICS_RES, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Attach texture to FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, causticsTexture, 0);
    
    // Set draw buffer once
    GLenum buf = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &buf);
    
    // Check FBO completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "FBO incomplete\n";
    }
    
    // Clear the caustics texture
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Reset to default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Main render loop
void renderLoop() {
    glm::vec3 lightPos(0.0f, 0.0f, 100.0f);
    
    auto startTime = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(window)) {
        // Calculate time for animations
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float>(currentTime - startTime).count();
        processInput(window);
        
        // Update water simulation
        update_wave();
        generateWaterMesh();
        glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, waterVertices.size() * sizeof(float), waterVertices.data());
        
        glClearColor(0.45f, 0.65f, 0.85f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::vec3 cameraPos(90.0f, -120.0f, 95.0f);
        glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, -10.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 projection = glm::perspective(glm::radians(50.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 800.0f);
        glm::mat4 model = glm::mat4(1.0f);

        // Caustics into their own texture, top-down, matching the floor UV.
        glBindFramebuffer(GL_FRAMEBUFFER, causticsFBO);
        glViewport(0, 0, CAUSTICS_RES, CAUSTICS_RES);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDisable(GL_DEPTH_TEST);

        float extent = (width / 2.0f) * waterScale;
        glm::mat4 causticsView = glm::lookAt(glm::vec3(0.0f, 0.0f, 80.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 causticsProj = glm::ortho(-extent, extent, -extent, extent, 0.1f, 300.0f);

        glUseProgram(causticsShaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(causticsShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(causticsShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(causticsView));
        glUniformMatrix4fv(glGetUniformLocation(causticsShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(causticsProj));
        glUniform3fv(glGetUniformLocation(causticsShaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform1f(glGetUniformLocation(causticsShaderProgram, "bottomZ"), BOTTOM_Z);
        glUniform1f(glGetUniformLocation(causticsShaderProgram, "waterIOR"), WATER_IOR);
        glUniform1f(glGetUniformLocation(causticsShaderProgram, "airIOR"), AIR_IOR);
        glUniform1f(glGetUniformLocation(causticsShaderProgram, "time"), time);

        glBindVertexArray(waterVAO);
        glDrawElements(GL_TRIANGLES, waterIndices.size(), GL_UNSIGNED_INT, 0);

        glDisable(GL_BLEND);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

        // Sky first. This GLAD has no glDepthFunc, so the sky cannot use the xyww depth=1 trick last.
        glDisable(GL_DEPTH_TEST);
        glUseProgram(skyboxShaderProgram);
        glm::mat4 skyboxView = glm::mat4(glm::mat3(view));
        glUniformMatrix4fv(glGetUniformLocation(skyboxShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(skyboxView));
        glUniformMatrix4fv(glGetUniformLocation(skyboxShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glBindVertexArray(skyboxVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glEnable(GL_DEPTH_TEST);

        glUseProgram(bottomShaderProgram);
        glm::mat4 bottomModel = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(bottomShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(bottomModel));
        glUniformMatrix4fv(glGetUniformLocation(bottomShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(bottomShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, causticsTexture);
        glUniform1i(glGetUniformLocation(bottomShaderProgram, "causticsTexture"), 0);
        glUniform1f(glGetUniformLocation(bottomShaderProgram, "time"), time);
        glBindVertexArray(bottomVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(waterShaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(waterShaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(waterShaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));
        glBindVertexArray(waterVAO);
        glDrawElements(GL_TRIANGLES, waterIndices.size(), GL_UNSIGNED_INT, 0);
        glDisable(GL_BLEND);
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

int main() {
    if (!initGL()) {
        return -1;
    }
    
    // Create and compile shaders
    waterShaderProgram = createShaderProgram(surfaceVertexShaderSource, waterFragmentShaderSource);
    skyboxShaderProgram = createShaderProgram(skyboxVertexShaderSource, skyboxFragmentShaderSource);
    causticsShaderProgram = createShaderProgram(surfaceVertexShaderSource, causticsFragmentShaderSource);
    bottomShaderProgram = createShaderProgram(surfaceVertexShaderSource, bottomFragmentShaderSource);
    
    // Initialize water simulation
    init_grid();
    add_disturbance(100, 100, 2.0f);
    add_disturbance(300, 300, 1.5f);
    add_disturbance(150, 250, 1.8f);
    
    // Generate and setup meshes
    generateWaterMesh();
    setupWaterBuffers();
    generateBottomMesh();
    generateSkyboxMesh();
    
    // No face cull. The skybox is drawn from the inside.
    glEnable(GL_DEPTH_TEST);

    // Setup caustics FBO
    setupCausticsFBO();
    
    // Start render loop
    renderLoop();
    
    // Cleanup
    glDeleteVertexArrays(1, &waterVAO);
    glDeleteBuffers(1, &waterVBO);
    glDeleteBuffers(1, &waterEBO);
    glDeleteVertexArrays(1, &bottomVAO);
    glDeleteBuffers(1, &bottomVBO);
    glDeleteBuffers(1, &bottomEBO);
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteProgram(waterShaderProgram);
    glDeleteProgram(skyboxShaderProgram);
    glDeleteProgram(causticsShaderProgram);
    glDeleteProgram(bottomShaderProgram);
    
    glfwTerminate();
    return 0;
}

// Callback function for window resize
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// Process input
void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// Mouse click callback for disturbances
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        // Convert screen coordinates to world coordinates
        float normalizedX = (float)xpos / SCR_WIDTH;  // 0 to 1
        float normalizedY = 1.0f - (float)ypos / SCR_HEIGHT; // 0 to 1 (invert y for OpenGL)

        // Map to grid coordinates 
        int gridX = static_cast<int>(normalizedX * width);
        int gridY = static_cast<int>(normalizedY * height);

        // Ensure coordinates are within bounds and add disturbance
        if (gridX >= 0 && gridX < width && gridY >= 0 && gridY < height) {
            add_disturbance(gridX, gridY, 5.0f); // Add a disturbance with a height of 5.0
        }
    }
}

// Create and compile shader program
unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return shaderProgram;
}