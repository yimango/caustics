#include <vector>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shaders.h"

#ifndef GL_RGB
#define GL_RGB 0x1907
#endif
#ifndef GL_UNSIGNED_BYTE
#define GL_UNSIGNED_BYTE 0x1401
#endif
#ifndef GL_PACK_ALIGNMENT
#define GL_PACK_ALIGNMENT 0x0D05
#endif
#ifndef GL_RED
#define GL_RED 0x1903
#endif
#ifndef GL_R32F
#define GL_R32F 0x822E
#endif
#ifndef GL_TEXTURE_WRAP_S
#define GL_TEXTURE_WRAP_S 0x2802
#endif
#ifndef GL_TEXTURE_WRAP_T
#define GL_TEXTURE_WRAP_T 0x2803
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

typedef void (APIENTRYP PFNGLREADPIXELSPROC)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
typedef void (APIENTRYP PFNGLPIXELSTOREIPROC)(GLenum, GLint);

PFNGLREADPIXELSPROC readPixelsFn = nullptr;
PFNGLPIXELSTOREIPROC pixelStoreFn = nullptr;

int width = 400;
int height = 400;
float dx = 1.0f;
float dt = 0.7f;
float c = 1.0f;
float damping = 0.01f;
float waterScale = 1.0f;
const float meshHeightScale = 8.0f;

std::vector<float> height_current;
std::vector<float> height_prev;
std::vector<float> height_next;

const float WATER_IOR = 1.33f;
const float AIR_IOR = 1.0f;

const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 900;
const int CAUSTICS_RES = 2048;
const float BOTTOM_Z = -30.0f;

GLFWwindow* window;
unsigned int waterVAO, waterVBO, waterEBO;
unsigned int bottomVAO, bottomVBO, bottomEBO;
int floorIndexCount = 0;
int bottomIndexCount = 0;
unsigned int skyboxVAO, skyboxVBO;
unsigned int causticsFBO, causticsTexture;
unsigned int heightTexture;
unsigned int waterShaderProgram;
unsigned int skyboxShaderProgram;
unsigned int causticsShaderProgram;
unsigned int bottomShaderProgram;

int framebufferWidth = (int)SCR_WIDTH;
int framebufferHeight = (int)SCR_HEIGHT;

std::vector<unsigned int> waterIndices;

glm::mat4 beautyView(1.0f);
glm::mat4 beautyProjection(1.0f);

const char* dumpPath = nullptr;
int dumpFrameCount = -1;

void update_wave() {
    for (int x = 1; x < width - 1; ++x) {
        for (int y = 1; y < height - 1; ++y) {
            int i = x * height + y;
            float C = height_current[i];
            float P = height_prev[i];
            float laplacian =
                height_current[(x + 1) * height + y] +
                height_current[(x - 1) * height + y] +
                height_current[x * height + (y + 1)] +
                height_current[x * height + (y - 1)] -
                4.0f * C;
            height_next[i] = (1.0f - damping) * (2.0f * C - P) +
                             (c * c * dt * dt / (dx * dx)) * laplacian;
        }
    }

    height_prev = height_current;
    height_current = height_next;
}

void add_disturbance(int x, int y, float amplitude) {
    if (x <= 0 || x >= width - 1 || y <= 0 || y >= height - 1) {
        return;
    }
    height_current[x * height + y] = amplitude;
}

void init_grid() {
    int count = width * height;
    height_current.assign(count, 0.0f);
    height_prev.assign(count, 0.0f);
    height_next.assign(count, 0.0f);
}

void buildWaterMesh() {
    std::vector<float> vertices(static_cast<size_t>(width) * static_cast<size_t>(height) * 3);
    size_t v = 0;
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            vertices[v++] = (x - width / 2.0f) * waterScale;
            vertices[v++] = (y - height / 2.0f) * waterScale;
            vertices[v++] = 0.0f;
        }
    }

    waterIndices.clear();
    waterIndices.reserve(static_cast<size_t>(width - 1) * static_cast<size_t>(height - 1) * 6);
    for (int x = 0; x < width - 1; ++x) {
        for (int y = 0; y < height - 1; ++y) {
            unsigned int topLeft = static_cast<unsigned int>(x * height + y);
            unsigned int topRight = topLeft + 1;
            unsigned int bottomLeft = static_cast<unsigned int>((x + 1) * height + y);
            unsigned int bottomRight = bottomLeft + 1;

            waterIndices.push_back(topLeft);
            waterIndices.push_back(bottomLeft);
            waterIndices.push_back(topRight);

            waterIndices.push_back(topRight);
            waterIndices.push_back(bottomLeft);
            waterIndices.push_back(bottomRight);
        }
    }

    glGenVertexArrays(1, &waterVAO);
    glGenBuffers(1, &waterVBO);
    glGenBuffers(1, &waterEBO);

    glBindVertexArray(waterVAO);
    glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, waterEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(waterIndices.size() * sizeof(unsigned int)), waterIndices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

void framebuffer_size_callback(GLFWwindow*, int fbW, int fbH) {
    framebufferWidth = fbW;
    framebufferHeight = fbH;
    glViewport(0, 0, fbW, fbH);
}

void processInput(GLFWwindow* win) {
    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(win, true);
    }
}

void mouse_button_callback(GLFWwindow* win, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) {
        return;
    }

    double xpos = 0.0;
    double ypos = 0.0;
    glfwGetCursorPos(win, &xpos, &ypos);

    int fbW = 0;
    int fbH = 0;
    glfwGetFramebufferSize(win, &fbW, &fbH);
    int winW = 0;
    int winH = 0;
    glfwGetWindowSize(win, &winW, &winH);
    if (fbW <= 0 || fbH <= 0 || winW <= 0 || winH <= 0) {
        return;
    }

    float cursorX = (float)xpos * (float)fbW / (float)winW;
    float cursorY = (float)fbH - (float)ypos * (float)fbH / (float)winH;
    glm::vec4 viewport(0.0f, 0.0f, (float)fbW, (float)fbH);
    glm::vec3 nearPoint = glm::unProject(glm::vec3(cursorX, cursorY, 0.0f), beautyView, beautyProjection, viewport);
    glm::vec3 farPoint = glm::unProject(glm::vec3(cursorX, cursorY, 1.0f), beautyView, beautyProjection, viewport);
    glm::vec3 ray = farPoint - nearPoint;
    if (ray.z == 0.0f) {
        return;
    }
    float t = -nearPoint.z / ray.z;
    if (t < 0.0f) {
        return;
    }
    glm::vec3 hit = nearPoint + ray * t;
    int gridX = (int)(hit.x / waterScale + width / 2);
    int gridY = (int)(hit.y / waterScale + height / 2);
    if (gridX >= 0 && gridX < width && gridY >= 0 && gridY < height) {
        add_disturbance(gridX, gridY, 5.0f);
    }
}

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
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    readPixelsFn = reinterpret_cast<PFNGLREADPIXELSPROC>(glfwGetProcAddress("glReadPixels"));
    pixelStoreFn = reinterpret_cast<PFNGLPIXELSTOREIPROC>(glfwGetProcAddress("glPixelStorei"));

    return true;
}

void appendBottomVertex(std::vector<float>& vertices, float x, float y, float z) {
    vertices.push_back(x);
    vertices.push_back(y);
    vertices.push_back(z);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(1.0f);
}

void appendBottomQuad(std::vector<unsigned int>& indices, unsigned int i0, unsigned int i1, unsigned int i2, unsigned int i3) {
    indices.push_back(i0);
    indices.push_back(i1);
    indices.push_back(i2);
    indices.push_back(i2);
    indices.push_back(i3);
    indices.push_back(i0);
}

void generateBottomMesh() {
    float bottomZ = BOTTOM_Z;
    float lipZ = 18.0f;
    float halfWidth = (width / 2.0f) * waterScale;
    float halfHeight = (height / 2.0f) * waterScale;

    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    appendBottomVertex(vertices, -halfWidth, -halfHeight, bottomZ);
    appendBottomVertex(vertices, halfWidth, -halfHeight, bottomZ);
    appendBottomVertex(vertices, halfWidth, halfHeight, bottomZ);
    appendBottomVertex(vertices, -halfWidth, halfHeight, bottomZ);
    appendBottomQuad(indices, 0, 1, 2, 3);
    floorIndexCount = (int)indices.size();

    unsigned int south = (unsigned int)(vertices.size() / 6);
    appendBottomVertex(vertices, -halfWidth, -halfHeight, bottomZ);
    appendBottomVertex(vertices, halfWidth, -halfHeight, bottomZ);
    appendBottomVertex(vertices, halfWidth, -halfHeight, lipZ);
    appendBottomVertex(vertices, -halfWidth, -halfHeight, lipZ);
    appendBottomQuad(indices, south, south + 1, south + 2, south + 3);

    unsigned int east = (unsigned int)(vertices.size() / 6);
    appendBottomVertex(vertices, halfWidth, -halfHeight, bottomZ);
    appendBottomVertex(vertices, halfWidth, halfHeight, bottomZ);
    appendBottomVertex(vertices, halfWidth, halfHeight, lipZ);
    appendBottomVertex(vertices, halfWidth, -halfHeight, lipZ);
    appendBottomQuad(indices, east, east + 1, east + 2, east + 3);

    unsigned int north = (unsigned int)(vertices.size() / 6);
    appendBottomVertex(vertices, halfWidth, halfHeight, bottomZ);
    appendBottomVertex(vertices, -halfWidth, halfHeight, bottomZ);
    appendBottomVertex(vertices, -halfWidth, halfHeight, lipZ);
    appendBottomVertex(vertices, halfWidth, halfHeight, lipZ);
    appendBottomQuad(indices, north, north + 1, north + 2, north + 3);

    unsigned int west = (unsigned int)(vertices.size() / 6);
    appendBottomVertex(vertices, -halfWidth, halfHeight, bottomZ);
    appendBottomVertex(vertices, -halfWidth, -halfHeight, bottomZ);
    appendBottomVertex(vertices, -halfWidth, -halfHeight, lipZ);
    appendBottomVertex(vertices, -halfWidth, halfHeight, lipZ);
    appendBottomQuad(indices, west, west + 1, west + 2, west + 3);

    bottomIndexCount = (int)indices.size();

    glGenVertexArrays(1, &bottomVAO);
    glGenBuffers(1, &bottomVBO);
    glGenBuffers(1, &bottomEBO);

    glBindVertexArray(bottomVAO);

    glBindBuffer(GL_ARRAY_BUFFER, bottomVBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bottomEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

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

void setupCausticsFBO() {
    glGenFramebuffers(1, &causticsFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, causticsFBO);

    glGenTextures(1, &causticsTexture);
    glBindTexture(GL_TEXTURE_2D, causticsTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, CAUSTICS_RES, CAUSTICS_RES, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, causticsTexture, 0);

    GLenum buf = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &buf);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "FBO incomplete\n";
    }

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void setupHeightTexture() {
    glGenTextures(1, &heightTexture);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, heightTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, height_current.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void uploadHeightTexture() {
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, heightTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_FLOAT, height_current.data());
}

void storeU16(unsigned char* dst, unsigned int value) {
    dst[0] = (unsigned char)(value & 255u);
    dst[1] = (unsigned char)((value >> 8) & 255u);
}

void storeU32(unsigned char* dst, unsigned int value) {
    dst[0] = (unsigned char)(value & 255u);
    dst[1] = (unsigned char)((value >> 8) & 255u);
    dst[2] = (unsigned char)((value >> 16) & 255u);
    dst[3] = (unsigned char)((value >> 24) & 255u);
}

bool captureWindowBmp(const char* path) {
    if (path == nullptr || framebufferWidth <= 0 || framebufferHeight <= 0 || readPixelsFn == nullptr || pixelStoreFn == nullptr) {
        std::cerr << "Window framebuffer is empty\n";
        return false;
    }

    int fbW = framebufferWidth;
    int fbH = framebufferHeight;
    std::vector<unsigned char> rgb(static_cast<size_t>(fbW) * static_cast<size_t>(fbH) * 3);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    pixelStoreFn(GL_PACK_ALIGNMENT, 1);
    readPixelsFn(0, 0, fbW, fbH, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    pixelStoreFn(GL_PACK_ALIGNMENT, 4);

    const int rowStride = (fbW * 3 + 3) & ~3;
    const int pixelBytes = rowStride * fbH;
    const int fileSize = 54 + pixelBytes;
    std::vector<unsigned char> file(fileSize, 0);
    file[0] = 'B';
    file[1] = 'M';
    storeU32(file.data() + 2, (unsigned int)fileSize);
    storeU32(file.data() + 10, 54);
    storeU32(file.data() + 14, 40);
    storeU32(file.data() + 18, (unsigned int)fbW);
    storeU32(file.data() + 22, (unsigned int)fbH);
    storeU16(file.data() + 26, 1);
    storeU16(file.data() + 28, 24);
    storeU32(file.data() + 34, (unsigned int)pixelBytes);

    for (int y = 0; y < fbH; ++y) {
        unsigned char* dst = file.data() + 54 + static_cast<size_t>(y) * rowStride;
        const unsigned char* src = rgb.data() + static_cast<size_t>(y) * fbW * 3;
        for (int x = 0; x < fbW; ++x) {
            dst[x * 3] = src[x * 3 + 2];
            dst[x * 3 + 1] = src[x * 3 + 1];
            dst[x * 3 + 2] = src[x * 3];
        }
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open " << path << "\n";
        return false;
    }
    out.write(reinterpret_cast<const char*>(file.data()), file.size());
    return (bool)out;
}

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

int renderLoop() {
    glm::vec3 lightPos(0.0f, 0.0f, 100.0f);
    glm::vec3 cameraPos(90.0f, -120.0f, 95.0f);
    int shown = 0;
    int exitCode = 0;

    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        update_wave();
        uploadHeightTexture();

        glClearColor(0.45f, 0.65f, 0.85f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        beautyView = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, -10.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        beautyProjection = glm::perspective(glm::radians(50.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 800.0f);
        glm::mat4 model(1.0f);

        glBindFramebuffer(GL_FRAMEBUFFER, causticsFBO);
        glViewport(0, 0, CAUSTICS_RES, CAUSTICS_RES);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

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
        glUniform1f(glGetUniformLocation(causticsShaderProgram, "heightScale"), meshHeightScale);
        glUniform1f(glGetUniformLocation(causticsShaderProgram, "poolExtent"), extent);
        float gridSizeValue[2] = { (float)width, (float)height };
        glUniform2fv(glGetUniformLocation(causticsShaderProgram, "gridSize"), 1, gridSizeValue);
        glUniform1i(glGetUniformLocation(causticsShaderProgram, "heightTex"), 1);

        glBindVertexArray(waterVAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)waterIndices.size(), GL_UNSIGNED_INT, 0);

        glDisable(GL_BLEND);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

        glDisable(GL_DEPTH_TEST);
        glUseProgram(skyboxShaderProgram);
        glm::mat4 skyboxView = glm::mat4(glm::mat3(beautyView));
        glUniformMatrix4fv(glGetUniformLocation(skyboxShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(skyboxView));
        glUniformMatrix4fv(glGetUniformLocation(skyboxShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(beautyProjection));
        glBindVertexArray(skyboxVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glEnable(GL_DEPTH_TEST);

        glUseProgram(bottomShaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(bottomShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(bottomShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(beautyView));
        glUniformMatrix4fv(glGetUniformLocation(bottomShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(beautyProjection));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, causticsTexture);
        glUniform1i(glGetUniformLocation(bottomShaderProgram, "causticsTexture"), 0);
        glUniform1f(glGetUniformLocation(bottomShaderProgram, "poolExtent"), extent);
        glUniform1f(glGetUniformLocation(bottomShaderProgram, "bottomZ"), BOTTOM_Z);
        glBindVertexArray(bottomVAO);
        glDrawElements(GL_TRIANGLES, floorIndexCount, GL_UNSIGNED_INT, 0);
        glDrawElements(GL_TRIANGLES, bottomIndexCount - floorIndexCount, GL_UNSIGNED_INT, (void*)(floorIndexCount * sizeof(unsigned int)));

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(waterShaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(beautyView));
        glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(beautyProjection));
        glUniform3fv(glGetUniformLocation(waterShaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(waterShaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));
        glUniform1f(glGetUniformLocation(waterShaderProgram, "heightScale"), meshHeightScale);
        glUniform1f(glGetUniformLocation(waterShaderProgram, "poolExtent"), extent);
        glUniform2fv(glGetUniformLocation(waterShaderProgram, "gridSize"), 1, gridSizeValue);
        glUniform1i(glGetUniformLocation(waterShaderProgram, "heightTex"), 1);
        glBindVertexArray(waterVAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)waterIndices.size(), GL_UNSIGNED_INT, 0);
        glDisable(GL_BLEND);

        bool dumpNow = dumpPath != nullptr && shown + 1 >= dumpFrameCount;
        if (dumpNow && !captureWindowBmp(dumpPath)) {
            exitCode = 1;
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
        ++shown;
        if (dumpNow) {
            break;
        }
    }

    return exitCode;
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--dump") == 0 && i + 1 < argc) {
            dumpPath = argv[++i];
            if (i + 1 < argc) {
                dumpFrameCount = std::atoi(argv[++i]);
            }
        }
    }
    if (dumpPath != nullptr && dumpFrameCount < 0) {
        dumpFrameCount = 180;
    }

    if (!initGL()) {
        return -1;
    }

    waterShaderProgram = createShaderProgram(surfaceVertexShaderSource, waterFragmentShaderSource);
    skyboxShaderProgram = createShaderProgram(skyboxVertexShaderSource, skyboxFragmentShaderSource);
    causticsShaderProgram = createShaderProgram(causticVertexShaderSource, causticsFragmentShaderSource);
    bottomShaderProgram = createShaderProgram(bottomVertexShaderSource, bottomFragmentShaderSource);

    init_grid();
    add_disturbance(100, 100, 2.0f);
    add_disturbance(300, 300, 1.5f);
    add_disturbance(150, 250, 1.8f);

    buildWaterMesh();
    generateBottomMesh();
    generateSkyboxMesh();

    glEnable(GL_DEPTH_TEST);
    setupCausticsFBO();
    setupHeightTexture();

    int exitCode = renderLoop();

    glDeleteVertexArrays(1, &waterVAO);
    glDeleteBuffers(1, &waterVBO);
    glDeleteBuffers(1, &waterEBO);
    glDeleteVertexArrays(1, &bottomVAO);
    glDeleteBuffers(1, &bottomVBO);
    glDeleteBuffers(1, &bottomEBO);
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteFramebuffers(1, &causticsFBO);
    glDeleteTextures(1, &causticsTexture);
    glDeleteTextures(1, &heightTexture);
    glDeleteProgram(waterShaderProgram);
    glDeleteProgram(skyboxShaderProgram);
    glDeleteProgram(causticsShaderProgram);
    glDeleteProgram(bottomShaderProgram);

    glfwTerminate();
    return exitCode;
}
