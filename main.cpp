#include <vector>
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdlib>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace std;

// Water simulation parameters
int width = 200;
int height = 200;
float dx = 1.0f;        // Grid spacing
float dt = 0.7f;        // Time step
float c = 1.0f;         // Wave speed
float damping = 0.01f;  // Damping factor
float waterScale = 2.0f; // Scale factor for water surface size

// Water height grids for simulation
std::vector<std::vector<float>> height_current(width, std::vector<float>(height));
std::vector<std::vector<float>> height_prev(width, std::vector<float>(height));
std::vector<std::vector<float>> height_next(width, std::vector<float>(height));

// Physical constants
const float WATER_IOR = 1.33f;  // Index of refraction for water
const float AIR_IOR = 1.0f;     // Index of refraction for air

// OpenGL window dimensions
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Function prototypes
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource);
void setupCausticsFBO();

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
    uniform sampler2D causticsTexture;
    
    void main() {
        // Create grid pattern
        float gridSize = 10.0; // Larger grid size
        vec2 grid = fract(FragPos.xy / gridSize);
        float gridLine = step(0.95, grid.x) + step(0.95, grid.y);
        
        // Base color (light gray)
        vec3 baseColor = vec3(0.8);
        
        // Grid lines (dark gray)
        vec3 finalColor = mix(baseColor, vec3(0.3), gridLine);
        
        // Sample caustics texture
        vec2 texCoord = (FragPos.xy + vec2(100.0)) / 200.0; // Map world space to texture space
        vec4 caustics = texture(causticsTexture, texCoord);
        
        // Amplify caustics
        float causticIntensity = caustics.a * 2.0; // Increase caustics brightness
        
        // Add caustics to the final color
        finalColor = finalColor + vec3(1.0) * causticIntensity;
        
        FragColor = vec4(finalColor, 1.0);
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
    
    void main() {
        // Create a nice gradient sky from blue to light blue
        float skyFactor = normalize(TexCoords).y * 0.5 + 0.5;
        vec3 skyColor = mix(vec3(0.5, 0.8, 1.0), vec3(0.8, 0.9, 1.0), skyFactor);
        FragColor = vec4(skyColor, 1.0);
    }
)";

// Add after other shader sources
const char* causticsVertexShaderSource = R"(
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

const char* causticsFragmentShaderSource = R"(
    #version 330 core
    in vec3 FragPos;
    in vec3 Normal;
    
    out vec4 FragColor;
    
    uniform vec3 lightPos;
    uniform float bottomZ;
    uniform float waterIOR;
    uniform float airIOR;
    uniform float time;
    
    void main() {
        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(lightPos - FragPos);
        
        // Simple caustic calculation based on surface curvature
        // Calculate the deviation of the normal from straight up
        vec3 upVector = vec3(0.0, 0.0, 1.0);
        float normalDeviation = length(norm - upVector);
        
        // Create caustic intensity based on how much the surface deviates from flat
        float causticIntensity = normalDeviation * 4.0; // Amplify the effect
        
        // Add some directional bias to avoid uniform distribution
        float directionBias = dot(norm.xy, vec2(1.0, 1.0)) * 0.5 + 0.5;
        causticIntensity *= directionBias;
        
        // Add time-based variation for movement
        float timeVar = sin(time * 1.5 + FragPos.x * 0.05 + FragPos.y * 0.05) * 0.4 + 0.6;
        causticIntensity *= timeVar;
        
        // Create some pattern variation
        float pattern = sin(FragPos.x * 0.1) * cos(FragPos.y * 0.1) * 0.3 + 0.7;
        causticIntensity *= pattern;
        
        // Clamp intensity
        causticIntensity = clamp(causticIntensity, 0.0, 1.0);
        
        // Output the caustic intensity
        FragColor = vec4(0.0, 0.0, 0.0, causticIntensity);
    }
)";

// Bottom surface shader
const char* bottomVertexShaderSource = R"(
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

const char* bottomFragmentShaderSource = R"(
    #version 330 core
    in vec3 FragPos;
    in vec3 Normal;
    
    out vec4 FragColor;
    
    uniform sampler2D causticsTexture;
    uniform float time;
    
    void main() {
        // Create a pool-style grid pattern
        float gridSize = 12.0;
        vec2 grid = fract(FragPos.xy / gridSize);
        float gridLine = smoothstep(0.85, 0.9, max(grid.x, grid.y));
        
        // Pool colors
        vec3 baseColor = vec3(0.1, 0.4, 0.7);  // Pool blue
        vec3 gridColor = vec3(0.3, 0.6, 0.9);  // Lighter pool blue
        
        // Mix base and grid colors
        vec3 finalColor = mix(baseColor, gridColor, gridLine * 0.4);
        
        // Sample caustics directly from the water surface position
        vec2 causticsUV = (FragPos.xy + vec2(200.0)) / 400.0;
        float causticIntensity = texture(causticsTexture, causticsUV).a;
        
        // Add multiple caustic layers with slight offsets for complexity
        vec2 offset1 = vec2(sin(time * 0.3) * 0.02, cos(time * 0.4) * 0.02);
        vec2 offset2 = vec2(cos(time * 0.7) * 0.03, sin(time * 0.6) * 0.03);
        
        float caustic1 = texture(causticsTexture, causticsUV + offset1).a;
        float caustic2 = texture(causticsTexture, causticsUV + offset2).a * 0.7;
        
        float totalCaustics = (causticIntensity + caustic1 + caustic2) * 2.5;
        
        // Apply caustics with bright, warm light
        vec3 causticColor = vec3(1.5, 1.2, 0.9); // Bright warm light
        finalColor += causticColor * totalCaustics;
        
        // Ensure proper color range
        finalColor = clamp(finalColor, 0.0, 1.2);
        
        FragColor = vec4(finalColor, 1.0);
    }
)";

// Water surface shader
const char* waterVertexShaderSource = R"(
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

const char* waterFragmentShaderSource = R"(
    #version 330 core
    in vec3 FragPos;
    in vec3 Normal;
    
    out vec4 FragColor;
    
    uniform vec3 lightPos;
    uniform vec3 viewPos;
    
    void main() {
        // Standard Phong lighting
        vec3 N = normalize(Normal);
        vec3 L = normalize(lightPos - FragPos);
        vec3 V = normalize(viewPos - FragPos);
        vec3 R = reflect(-L, N);
        
        float diff = max(dot(N, L), 0.0);
        float spec = pow(max(dot(V, R), 0.0), 32.0);
        
        // Ambient, diffuse, and specular components
        vec3 ambient = vec3(0.1);
        vec3 diffuse = vec3(0.4) * diff;
        vec3 specular = vec3(0.3) * spec;
        
        // Combine lighting components
        vec3 result = ambient + diffuse + specular;
        
        FragColor = vec4(result, 0.5); // Semi-transparent
    }
)";



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

// Add Ray struct for GLM
struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
    Ray(const glm::vec3& o, const glm::vec3& d) : origin(o), direction(glm::normalize(d)) {}
};

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
            height_next[i][j] = (1 - damping) * (2 * height_current[i][j] - height_prev[i][j]) +
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
glm::vec3 getSurfaceNormal(int x, int y) {
    float ddx = (height_current[x+1][y] - height_current[x-1][y]) / (2.0f * dx);
    float ddy = (height_current[x][y+1] - height_current[x][y-1]) / (2.0f * dx);
    glm::vec3 n(-ddx, -ddy, 1.0f);
    return glm::normalize(n);
}

// Function to calculate refraction direction
glm::vec3 refract(const glm::vec3& I, const glm::vec3& N, float ior) {
    float cosi = -glm::dot(I, N);
    float etai = 1.0f, etat = ior;
    glm::vec3 n = N;
    if (cosi < 0) {
        cosi = -cosi;
        std::swap(etai, etat);
        n = -N;
    }
    float eta = etai / etat;
    float k = 1 - eta * eta * (1 - cosi * cosi);
    if (k < 0) return glm::vec3(0,0,0);
    return I * eta + n * (eta * cosi - sqrt(k));
}

// Ray tracing function (unused - kept for reference)
glm::vec3 traceRay(const Ray& ray, int depth = 0) {
    if (depth > 5) return glm::vec3(0,0,0);
    float t = -ray.origin.z / ray.direction.z;
    if (t < 0) return glm::vec3(0,0,0);
    glm::vec3 hitPoint = ray.origin + ray.direction * t;
    int x = static_cast<int>(hitPoint.x);
    int y = static_cast<int>(hitPoint.y);
    if (x < 1 || x >= width-1 || y < 1 || y >= height-1)
        return glm::vec3(0,0,0);
    glm::vec3 normal = getSurfaceNormal(x, y);
    glm::vec3 refrDir = refract(ray.direction, normal, WATER_IOR);
    if (glm::length(refrDir) < 0.001f) {
        return glm::vec3(0,0,0);
    }
    Ray refrRay(hitPoint, refrDir);
    glm::vec3 refrColor = traceRay(refrRay, depth + 1);
    float causticIntensity = pow(1.0f - abs(glm::dot(normal, ray.direction)), 4.0f);
    return refrColor * (1.0f - causticIntensity) + glm::vec3(1,1,1) * causticIntensity;
}

// Function to render the scene
void renderScene() {
    const int imageWidth = width;
    const int imageHeight = height;
    glm::vec3 cameraPos(0, 0, -10);
    float fov = 60.0f;
    float aspectRatio = float(imageWidth) / float(imageHeight);
    for (int y = 0; y < imageHeight; ++y) {
        for (int x = 0; x < imageWidth; ++x) {
            float px = (2.0f * ((x + 0.5f) / imageWidth) - 1.0f) * aspectRatio * tan(fov * 0.5f * M_PI / 180.0f);
            float py = (1.0f - 2.0f * ((y + 0.5f) / imageHeight)) * tan(fov * 0.5f * M_PI / 180.0f);
            glm::vec3 rayDir(px, py, 1.0f);
            Ray ray(cameraPos, rayDir);
            glm::vec3 color = traceRay(ray);
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
            // Position (scaled to fill more of the viewport)
            waterVertices.push_back((i - width/2.0f) * waterScale);
            waterVertices.push_back((j - height/2.0f) * waterScale);
            waterVertices.push_back(height_current[i][j]);
            
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
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
    glm::vec3 lightColor(1.0f, 1.0f, 1.0f);
    
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
        
        // Clear screen
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Camera setup (positioned to view the larger water surface)
        glm::vec3 cameraPos(0.0f, 0.0f, 80.0f);
        glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 projection = glm::perspective(glm::radians(60.0f), (float)SCR_WIDTH/SCR_HEIGHT, 0.1f, 300.0f);

        // 2. Generate Caustics Texture
        glBindFramebuffer(GL_FRAMEBUFFER, causticsFBO);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE); // Additive blending for caustics accumulation
        glDisable(GL_DEPTH_TEST);    // Disable depth test for caustics pass
        
        // Use the same projection as the main camera for consistency
        glUseProgram(causticsShaderProgram);
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(causticsShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(causticsShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(causticsShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(causticsShaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform1f(glGetUniformLocation(causticsShaderProgram, "bottomZ"), BOTTOM_Z);
        glUniform1f(glGetUniformLocation(causticsShaderProgram, "waterIOR"), WATER_IOR);
        glUniform1f(glGetUniformLocation(causticsShaderProgram, "airIOR"), AIR_IOR);
        glUniform1f(glGetUniformLocation(causticsShaderProgram, "time"), time);
        
        glBindVertexArray(waterVAO);
        glDrawElements(GL_TRIANGLES, waterIndices.size(), GL_UNSIGNED_INT, 0);
        
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST); // Re-enable depth test
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 3. Render Pool Bottom (with caustics)
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

        // 4. Render Water Surface (transparent)
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

        // 5. Render Skybox (background)
        glDepthMask(GL_FALSE);
        glUseProgram(skyboxShaderProgram);
        glm::mat4 skyboxView = glm::mat4(glm::mat3(view));
        glUniformMatrix4fv(glGetUniformLocation(skyboxShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(skyboxView));
        glUniformMatrix4fv(glGetUniformLocation(skyboxShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glBindVertexArray(skyboxVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthMask(GL_TRUE);
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

int main() {
    if (!initGL()) {
        return -1;
    }
    
    // Create and compile shaders
    waterShaderProgram = createShaderProgram(waterVertexShaderSource, waterFragmentShaderSource);
    skyboxShaderProgram = createShaderProgram(skyboxVertexShaderSource, skyboxFragmentShaderSource);
    causticsShaderProgram = createShaderProgram(causticsVertexShaderSource, causticsFragmentShaderSource);
    bottomShaderProgram = createShaderProgram(bottomVertexShaderSource, bottomFragmentShaderSource);
    
    // Initialize water simulation
    init_grid();
    add_disturbance(50, 50, 2.0f);
    add_disturbance(150, 150, 1.5f);
    add_disturbance(75, 125, 1.8f);
    
    // Generate and setup meshes
    generateWaterMesh();
    setupWaterBuffers();
    generateBottomMesh();
    generateSkyboxMesh();
    
    // Enable depth testing and face culling
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE); // Enable face culling for performance
    glCullFace(GL_BACK); // Cull back faces

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