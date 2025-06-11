#include <vector>
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace std;

int width = 200;
int height = 200;
float dx = 1.0f;        // Grid spacing
float dt = 0.03f;       // Time step
float c = 1.0f;
float damping = 0.01f;         // Wave speed

std::vector<std::vector<float>> height_current(width, std::vector<float>(height));
std::vector<std::vector<float>> height_prev(width, std::vector<float>(height));
std::vector<std::vector<float>> height_next(width, std::vector<float>(height));

// Constants for ray tracing
const float WATER_IOR = 1.33f;  // Index of refraction for water
const float AIR_IOR = 1.0f;     // Index of refraction for air
const int MAX_RAY_DEPTH = 5;    // Maximum number of ray bounces

// OpenGL window dimensions
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Shader sources
const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aNormal;
    
    out vec3 FragPos;
    out vec3 Normal;
    
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    
    void main() {
        FragPos = vec3(model * vec4(aPos, 1.0));
        Normal = mat3(transpose(inverse(model))) * aNormal;
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    in vec3 FragPos;
    in vec3 Normal;
    
    out vec4 FragColor;
    
    uniform vec3 lightPos;
    uniform vec3 viewPos;
    uniform vec3 lightColor;
    
    void main() {
        // Ambient
        float ambientStrength = 0.1;
        vec3 ambient = ambientStrength * lightColor;
        
        // Diffuse
        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(lightPos - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * lightColor;
        
        // Specular
        float specularStrength = 0.5;
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
        vec3 specular = specularStrength * spec * lightColor;
        
        // Caustics (simplified - will be projected onto bottom surface later)
        float causticIntensity = pow(1.0 - abs(dot(norm, lightDir)), 4.0);
        vec3 caustics = vec3(1.0) * causticIntensity; // Placeholder for now
        
        vec3 result = (ambient + diffuse + specular);
        FragColor = vec4(result, 1.0);
    }
)";

const char* skyboxVertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    
    out vec3 TexCoords;
    
    uniform mat4 projection;
    uniform mat4 view;
    
    void main() {
        TexCoords = aPos;
        vec4 pos = projection * view * vec4(aPos, 1.0);
        gl_Position = pos.xyww; // Ensure z is 1.0 for skybox to be infinitely far
    }
)";

const char* skyboxFragmentShaderSource = R"(
    #version 330 core
    in vec3 TexCoords;
    
    out vec4 FragColor;
    
    uniform samplerCube skybox;
    
    void main() {
        // Placeholder skybox colors for now
        FragColor = vec4(TexCoords.x * 0.5 + 0.5, TexCoords.y * 0.5 + 0.5, TexCoords.z * 0.5 + 0.5, 1.0);
    }
)";

// Function prototypes
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource);

// Global variables
GLFWwindow* window;
unsigned int waterShaderProgram;
unsigned int skyboxShaderProgram;
unsigned int waterVAO, waterVBO, waterEBO;
unsigned int bottomVAO, bottomVBO, bottomEBO;
unsigned int skyboxVAO, skyboxVBO;

std::vector<float> waterVertices;
std::vector<unsigned int> waterIndices;

// fluid simulation logic
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
            height_next[i][j] = damping * (2 * height_current[i][j] - height_prev[i][j]) +
                                (c * c * dt * dt / (dx * dx)) * laplacian;
        }
    }

    // Swap buffers
    height_prev = height_current;
    height_current = height_next;
}

void add_disturbance(int x, int y, float height){
    height_current[x][y] = height;
}

void init_grid(){
    for(int i = 0; i < width; i++){
        for(int j = 0; j < height; j++){
            height_current[i][j] = 0.0f;
        }
    }
}

// Function to get surface normal at a point
Vector3 getSurfaceNormal(int x, int y) {
    // Calculate normal using central differences
    float dx = (height_current[x+1][y] - height_current[x-1][y]) / (2.0f * dx);
    float dy = (height_current[x][y+1] - height_current[x][y-1]) / (2.0f * dx);
    return Vector3(-dx, -dy, 1.0f).normalize();
}

// Function to calculate refraction direction
Vector3 refract(const Vector3& I, const Vector3& N, float ior) {
    float cosi = -I.dot(N);
    float etai = 1.0f, etat = ior;
    Vector3 n = N;
    
    if (cosi < 0) {
        cosi = -cosi;
        std::swap(etai, etat);
        n = N * -1.0f;
    }
    
    float eta = etai / etat;
    float k = 1 - eta * eta * (1 - cosi * cosi);
    
    return k < 0 ? Vector3(0,0,0) : I * eta + n * (eta * cosi - sqrt(k));
}

// Ray tracing function
Vector3 traceRay(const Ray& ray, int depth = 0) {
    if (depth > MAX_RAY_DEPTH) return Vector3(0,0,0);
    
    // Find intersection with water surface
    float t = -ray.origin.z / ray.direction.z;
    if (t < 0) return Vector3(0,0,0);  // Ray goes up
    
    Vector3 hitPoint = ray.origin + ray.direction * t;
    int x = static_cast<int>(hitPoint.x);
    int y = static_cast<int>(hitPoint.y);
    
    if (x < 1 || x >= width-1 || y < 1 || y >= height-1) 
        return Vector3(0,0,0);
    
    // Get surface normal at hit point
    Vector3 normal = getSurfaceNormal(x, y);
    
    // Calculate refraction
    Vector3 refrDir = refract(ray.direction, normal, WATER_IOR);
    if (refrDir.length() < 0.001f) {
        // Total internal reflection
        return Vector3(0,0,0);
    }
    
    // Trace refracted ray
    Ray refrRay(hitPoint, refrDir);
    Vector3 refrColor = traceRay(refrRay, depth + 1);
    
    // Calculate caustics (simplified)
    float causticIntensity = pow(1.0f - abs(normal.dot(ray.direction)), 4.0f);
    
    return refrColor * (1.0f - causticIntensity) + Vector3(1,1,1) * causticIntensity;
}

// Function to render the scene
void renderScene() {
    const int imageWidth = width;
    const int imageHeight = height;
    
    // Camera setup
    Vector3 cameraPos(0, 0, -10);
    float fov = 60.0f;
    float aspectRatio = float(imageWidth) / float(imageHeight);
    
    // Render loop
    for (int y = 0; y < imageHeight; ++y) {
        for (int x = 0; x < imageWidth; ++x) {
            // Calculate ray direction
            float px = (2.0f * ((x + 0.5f) / imageWidth) - 1.0f) * aspectRatio * tan(fov * 0.5f * M_PI / 180.0f);
            float py = (1.0f - 2.0f * ((y + 0.5f) / imageHeight)) * tan(fov * 0.5f * M_PI / 180.0f);
            Vector3 rayDir(px, py, 1.0f);
            
            // Create and trace ray
            Ray ray(cameraPos, rayDir);
            Vector3 color = traceRay(ray);
            
            // Output color (you might want to save this to an image file)
            cout << color.x << " " << color.y << " " << color.z << " ";
        }
        cout << endl;
    }
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
            // Position
            waterVertices.push_back(i - width/2.0f);
            waterVertices.push_back(j - height/2.0f);
            waterVertices.push_back(height_current[i][j]);
            
            // Normal (will be updated in shader or calculated from surrounding vertices)
            waterVertices.push_back(0.0f);
            waterVertices.push_back(0.0f);
            waterVertices.push_back(1.0f);
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
    float bottom_y = -20.0f; // Y-coordinate of the bottom surface
    float half_width = width / 2.0f;
    float half_height = height / 2.0f;

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

// Main render loop
void renderLoop() {
    glm::vec3 lightPos(0.0f, -50.0f, 30.0f); // Light source position (same as camera for now)
    glm::vec3 lightColor(1.0f, 1.0f, 1.0f); // White light

    while (!glfwWindowShouldClose(window)) {
        processInput(window);
        
        // Update water simulation
        update_wave();
        
        // Update water vertex positions
        for (int i = 0; i < width; i++) {
            for (int j = 0; j < height; j++) {
                int vertexIndex = (i * height + j) * 6;  // 6 floats per vertex (pos + normal)
                waterVertices[vertexIndex + 2] = height_current[i][j];  // Update z position
            }
        }
        
        // Update VBO for water surface
        glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, waterVertices.size() * sizeof(float), waterVertices.data());
        
        // Clear screen
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        
        // Camera setup
        glm::vec3 cameraPos(0.0f, -50.0f, 30.0f); // Camera position
        glm::mat4 view = glm::lookAt(
            cameraPos,                  // Camera position
            glm::vec3(0.0f, 0.0f, 0.0f),     // Look at point
            glm::vec3(0.0f, 0.0f, 1.0f)      // Up vector
        );
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH/SCR_HEIGHT, 0.1f, 100.0f);

        // 1. Render Skybox
        glDepthMask(GL_FALSE);
        glUseProgram(skyboxShaderProgram);
        glm::mat4 skyboxView = glm::mat4(glm::mat3(view)); // Remove translation from the view matrix
        glUniformMatrix4fv(glGetUniformLocation(skyboxShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(skyboxView));
        glUniformMatrix4fv(glGetUniformLocation(skyboxShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glBindVertexArray(skyboxVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthMask(GL_TRUE);

        // 2. Render Water Surface
        glUseProgram(waterShaderProgram);
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(waterShaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(waterShaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));
        glUniform3fv(glGetUniformLocation(waterShaderProgram, "lightColor"), 1, glm::value_ptr(lightColor));
        
        glBindVertexArray(waterVAO);
        glDrawElements(GL_TRIANGLES, waterIndices.size(), GL_UNSIGNED_INT, 0);

        // 3. Render Bottom Surface
        // We'll use the same shader for simplicity, can be extended for caustics projection
        glUseProgram(waterShaderProgram); 
        glm::mat4 bottomModel = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(bottomModel));
        glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(waterShaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(waterShaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));
        glUniform3fv(glGetUniformLocation(waterShaderProgram, "lightColor"), 1, glm::value_ptr(glm::vec3(0.5f, 0.5f, 0.5f))); // Grey color for bottom
        
        glBindVertexArray(bottomVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0); // 6 indices for a quad
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

int main() {
    if (!initGL()) {
        return -1;
    }
    
    // Create and compile shaders
    waterShaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    skyboxShaderProgram = createShaderProgram(skyboxVertexShaderSource, skyboxFragmentShaderSource);
    
    // Initialize water simulation
    init_grid();
    add_disturbance(50, 50, 1.0f);
    add_disturbance(52, 52, 1.0f);
    
    // Generate and setup meshes
    generateWaterMesh();
    setupWaterBuffers();
    generateBottomMesh();
    generateSkyboxMesh();
    
    // Enable depth testing and face culling
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE); // Enable face culling for performance
    glCullFace(GL_BACK); // Cull back faces

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

        // Convert screen coordinates to world coordinates (simplified for flat plane)
        // This conversion assumes a projection where z=0 is the water plane
        // and camera is looking down. More accurate projection needed for real 3D picking.
        float normalizedX = (float)xpos / SCR_WIDTH;  // 0 to 1
        float normalizedY = 1.0f - (float)ypos / SCR_HEIGHT; // 0 to 1 (invert y for OpenGL)

        // Map to grid coordinates (adjusting for center offset and grid dimensions)
        int gridX = static_cast<int>(normalizedX * width);
        int gridY = static_cast<int>(normalizedY * height);

        // Ensure coordinates are within bounds and add disturbance
        if (gridX >= 0 && gridX < width && gridY >= 0 && gridY < height) {
            add_disturbance(gridX, gridY, 5.0f); // Add a disturbance with a height of 5.0
            std::cout << "Disturbance added at grid: (" << gridX << ", " << gridY << ")" << std::endl;
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