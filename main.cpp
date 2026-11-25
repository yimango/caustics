#include <vector>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <cmath>
#include <cstdio>
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
unsigned int causticsBlurFBO, causticsBlurTexture;
unsigned int causticBlurVAO;
unsigned int heightTexture;
unsigned int waterShaderProgram;
unsigned int skyboxShaderProgram;
unsigned int causticsShaderProgram;
unsigned int causticBlurShaderProgram;
unsigned int bottomShaderProgram;

int framebufferWidth = (int)SCR_WIDTH;
int framebufferHeight = (int)SCR_HEIGHT;

int axisX = 0;
int axisY = 0;
std::vector<unsigned int> waterIndices;

glm::mat4 beautyView(1.0f);
glm::mat4 beautyProjection(1.0f);

const char* dumpPath = nullptr;
int dumpFrameCount = -1;

struct BaseDisturbance {
    float amplitude;
    float wavelength;
    float tempo;
};

struct Slider {
    const char* label;
    float minValue;
    float maxValue;
    float* value;
};

BaseDisturbance baseDisturbance = {0.01f, 40.0f, 0.05f};

Slider sliders[] = {
    {"AMP", 0.0f, 0.04f, &baseDisturbance.amplitude},
    {"SCALE", 16.0f, 80.0f, &baseDisturbance.wavelength},
    {"TEMPO", 0.0f, 0.20f, &baseDisturbance.tempo},
};

const int kSliderCount = 3;
const float kPanelX = 16.0f;
const float kPanelW = 300.0f;
const float kPanelPad = 8.0f;
const float kRowH = 32.0f;
const float kGlyphScale = 3.0f;
const float kLabelColumn = 96.0f;
const float kValueColumn = 96.0f;

int sliderDrag = -1;
bool leftButtonDown = false;
int dropGridX = 0;
int dropGridY = 0;
float dropAmplitude = 0.0f;
int dropFrame = -1;

unsigned int overlayVAO = 0;
unsigned int overlayVBO = 0;
unsigned int overlayShaderProgram = 0;
int overlayColorLoc = -1;
int overlayAlphaLoc = -1;
int overlayViewportLoc = -1;

struct Glyph {
    char code;
    unsigned char rows[7];
};

const Glyph glyphs[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
    {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'2', {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F}},
    {'3', {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}},
    {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
    {'5', {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}},
    {'6', {0x0E, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x0E}},
    {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
    {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E}},
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06}},
    {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
};

const int glyphCount = (int)(sizeof(glyphs) / sizeof(glyphs[0]));

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
    const int meshSubdiv = 2;
    axisX = (width - 1) * meshSubdiv + 1;
    axisY = (height - 1) * meshSubdiv + 1;

    std::vector<float> vertices(static_cast<size_t>(axisX) * static_cast<size_t>(axisY) * 3);
    size_t v = 0;
    for (int i = 0; i < axisX; ++i) {
        float gx = static_cast<float>(i) * static_cast<float>(width - 1) / static_cast<float>(axisX - 1);
        for (int j = 0; j < axisY; ++j) {
            float gy = static_cast<float>(j) * static_cast<float>(height - 1) / static_cast<float>(axisY - 1);
            vertices[v++] = (gx - width / 2.0f) * waterScale;
            vertices[v++] = (gy - height / 2.0f) * waterScale;
            vertices[v++] = 0.0f;
        }
    }

    waterIndices.clear();
    waterIndices.reserve(static_cast<size_t>(axisX - 1) * static_cast<size_t>(axisY - 1) * 6);
    for (int i = 0; i < axisX - 1; ++i) {
        for (int j = 0; j < axisY - 1; ++j) {
            unsigned int topLeft = static_cast<unsigned int>(i * axisY + j);
            unsigned int topRight = topLeft + 1;
            unsigned int bottomLeft = static_cast<unsigned int>((i + 1) * axisY + j);
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

void apply_base_disturbance(int frame) {
    float k = 6.2831853f / baseDisturbance.wavelength;
    float envelope = std::cos((float)frame * baseDisturbance.tempo);
    for (int x = 1; x < width - 1; ++x) {
        for (int y = 1; y < height - 1; ++y) {
            height_current[x * height + y] += baseDisturbance.amplitude * std::sin(x * k + y * k * 0.37f) * envelope;
        }
    }
}

float panelTopY() {
    return (float)SCR_HEIGHT - 12.0f;
}

float panelHeight() {
    return kPanelPad * 2.0f + kRowH * (float)kSliderCount;
}

float panelBottomY() {
    return panelTopY() - panelHeight();
}

float rowBottomY(int index) {
    return panelTopY() - kPanelPad - kRowH * (float)(index + 1);
}

float sliderTrackX() {
    return kPanelX + kPanelPad + kLabelColumn;
}

float sliderValueX() {
    return kPanelX + kPanelW - kPanelPad - kValueColumn;
}

float sliderTrackW() {
    return sliderValueX() - 8.0f - sliderTrackX();
}

int sliderRowAt(float x, float y) {
    if (x < kPanelX || x >= kPanelX + kPanelW || y < panelBottomY() || y >= panelTopY()) {
        return -1;
    }
    for (int i = 0; i < kSliderCount; ++i) {
        float bottom = rowBottomY(i);
        if (y >= bottom && y < bottom + kRowH) {
            return i;
        }
    }
    if (y >= panelTopY() - kPanelPad) {
        return 0;
    }
    return kSliderCount - 1;
}

void writeSliderAt(int index, float cursorX) {
    float span = sliderTrackW();
    float t = 0.0f;
    if (span > 0.0f) {
        t = (cursorX - sliderTrackX()) / span;
    }
    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }
    Slider* slider = &sliders[index];
    *slider->value = slider->minValue + t * (slider->maxValue - slider->minValue);
}

bool readCursor(GLFWwindow* win, float* fbX, float* fbY, float* viewX, float* viewY, int* outFbW, int* outFbH) {
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
        return false;
    }
    float cursorX = (float)xpos * (float)fbW / (float)winW;
    float cursorY = (float)fbH - (float)ypos * (float)fbH / (float)winH;
    *fbX = cursorX;
    *fbY = cursorY;
    *viewX = cursorX * (float)SCR_WIDTH / (float)fbW;
    *viewY = cursorY * (float)SCR_HEIGHT / (float)fbH;
    *outFbW = fbW;
    *outFbH = fbH;
    return true;
}

void trackSliderDrag() {
    if (!leftButtonDown || sliderDrag < 0) {
        return;
    }
    float fbX = 0.0f;
    float fbY = 0.0f;
    float viewX = 0.0f;
    float viewY = 0.0f;
    int fbW = 0;
    int fbH = 0;
    if (!readCursor(window, &fbX, &fbY, &viewX, &viewY, &fbW, &fbH)) {
        return;
    }
    writeSliderAt(sliderDrag, viewX);
}

const unsigned char* findGlyph(char code) {
    for (int i = 0; i < glyphCount; ++i) {
        if (glyphs[i].code == code) {
            return glyphs[i].rows;
        }
    }
    return glyphs[0].rows;
}

void initOverlayMesh() {
    glGenVertexArrays(1, &overlayVAO);
    glGenBuffers(1, &overlayVBO);
    glBindVertexArray(overlayVAO);
    glBindBuffer(GL_ARRAY_BUFFER, overlayVBO);
    glBufferData(GL_ARRAY_BUFFER, 12 * (GLsizeiptr)sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

void drawOverlayRect(float x, float y, float w, float h, float r, float g, float b, float a) {
    float verts[12] = {
        x, y,
        x + w, y,
        x + w, y + h,
        x, y,
        x + w, y + h,
        x, y + h
    };
    glBindBuffer(GL_ARRAY_BUFFER, overlayVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    float color[3] = {r, g, b};
    glUniform3fv(overlayColorLoc, 1, color);
    glUniform1f(overlayAlphaLoc, a);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void drawGlyph(char code, float x, float y, float scale, float r, float g, float b) {
    const unsigned char* rows = findGlyph(code);
    for (int row = 0; row < 7; ++row) {
        unsigned char bits = rows[row];
        for (int col = 0; col < 5; ++col) {
            if ((bits & (unsigned char)(0x10 >> col)) == 0) {
                continue;
            }
            drawOverlayRect(x + (float)col * scale, y + (float)(6 - row) * scale, scale, scale, r, g, b, 1.0f);
        }
    }
}

void drawText(const char* text, float x, float y, float scale, float r, float g, float b) {
    float pen = x;
    for (int i = 0; text[i] != '\0'; ++i) {
        drawGlyph(text[i], pen, y, scale, r, g, b);
        pen += 6.0f * scale;
    }
}

void drawSliders() {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(overlayShaderProgram);
    float viewportPx[2] = {(float)SCR_WIDTH, (float)SCR_HEIGHT};
    glUniform2fv(overlayViewportLoc, 1, viewportPx);
    glBindVertexArray(overlayVAO);
    drawOverlayRect(kPanelX, panelBottomY(), kPanelW, panelHeight(), 0.06f, 0.08f, 0.09f, 0.82f);
    for (int i = 0; i < kSliderCount; ++i) {
        float bottom = rowBottomY(i);
        float glyphY = bottom + (kRowH - 7.0f * kGlyphScale) * 0.5f;
        drawText(sliders[i].label, kPanelX + kPanelPad, glyphY, kGlyphScale, 0.86f, 0.90f, 0.88f);
        float trackX = sliderTrackX();
        float trackW = sliderTrackW();
        float trackH = 6.0f;
        float trackY = bottom + (kRowH - trackH) * 0.5f;
        drawOverlayRect(trackX, trackY, trackW, trackH, 0.18f, 0.22f, 0.24f, 1.0f);
        float minValue = sliders[i].minValue;
        float maxValue = sliders[i].maxValue;
        float t = 0.0f;
        if (maxValue > minValue) {
            t = (*sliders[i].value - minValue) / (maxValue - minValue);
        }
        if (t < 0.0f) {
            t = 0.0f;
        }
        if (t > 1.0f) {
            t = 1.0f;
        }
        drawOverlayRect(trackX, trackY, trackW * t, trackH, 0.45f, 0.66f, 0.60f, 1.0f);
        float thumbW = 8.0f;
        float thumbH = 18.0f;
        float thumbX = trackX + trackW * t - thumbW * 0.5f;
        float thumbY = bottom + (kRowH - thumbH) * 0.5f;
        drawOverlayRect(thumbX, thumbY, thumbW, thumbH, 0.93f, 0.95f, 0.92f, 1.0f);
        char valueText[16];
        const char* format = "%.3f";
        if (sliders[i].value == &baseDisturbance.wavelength) {
            format = "%.1f";
        }
        std::snprintf(valueText, sizeof(valueText), format, *sliders[i].value);
        drawText(valueText, sliderValueX(), glyphY, kGlyphScale, 0.86f, 0.90f, 0.88f);
    }
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void mouse_button_callback(GLFWwindow* win, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    if (action == GLFW_RELEASE) {
        leftButtonDown = false;
        sliderDrag = -1;
        return;
    }
    if (action != GLFW_PRESS) {
        return;
    }
    leftButtonDown = true;

    float fbX = 0.0f;
    float fbY = 0.0f;
    float viewX = 0.0f;
    float viewY = 0.0f;
    int fbW = 0;
    int fbH = 0;
    if (!readCursor(win, &fbX, &fbY, &viewX, &viewY, &fbW, &fbH)) {
        return;
    }
    int row = sliderRowAt(viewX, viewY);
    if (row >= 0) {
        sliderDrag = row;
        float trackX = sliderTrackX();
        float trackW = sliderTrackW();
        if (viewX >= trackX && viewX <= trackX + trackW) {
            writeSliderAt(row, viewX);
        }
        return;
    }

    glm::vec4 viewport(0.0f, 0.0f, (float)fbW, (float)fbH);
    glm::vec3 nearPoint = glm::unProject(glm::vec3(fbX, fbY, 0.0f), beautyView, beautyProjection, viewport);
    glm::vec3 farPoint = glm::unProject(glm::vec3(fbX, fbY, 1.0f), beautyView, beautyProjection, viewport);
    glm::vec3 ray = farPoint - nearPoint;
    if (ray.z == 0.0f) {
        return;
    }
    float travel = -nearPoint.z / ray.z;
    if (travel < 0.0f) {
        return;
    }
    glm::vec3 hit = nearPoint + ray * travel;
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

    glGenFramebuffers(1, &causticsBlurFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, causticsBlurFBO);
    glGenTextures(1, &causticsBlurTexture);
    glBindTexture(GL_TEXTURE_2D, causticsBlurTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, CAUSTICS_RES, CAUSTICS_RES, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, causticsBlurTexture, 0);
    glDrawBuffers(1, &buf);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "FBO incomplete\n";
    }
    glGenVertexArrays(1, &causticBlurVAO);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void blurCausticMap() {
    glDisable(GL_BLEND);
    glUseProgram(causticBlurShaderProgram);
    glBindVertexArray(causticBlurVAO);
    glUniform1i(glGetUniformLocation(causticBlurShaderProgram, "source"), 0);
    glActiveTexture(GL_TEXTURE0);

    float horizontal[2] = {1.0f / (float)CAUSTICS_RES, 0.0f};
    float vertical[2] = {0.0f, 1.0f / (float)CAUSTICS_RES};
    glBindFramebuffer(GL_FRAMEBUFFER, causticsBlurFBO);
    glBindTexture(GL_TEXTURE_2D, causticsTexture);
    glUniform2fv(glGetUniformLocation(causticBlurShaderProgram, "direction"), 1, horizontal);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindFramebuffer(GL_FRAMEBUFFER, causticsFBO);
    glBindTexture(GL_TEXTURE_2D, causticsBlurTexture);
    glUniform2fv(glGetUniformLocation(causticBlurShaderProgram, "direction"), 1, vertical);
    glDrawArrays(GL_TRIANGLES, 0, 3);
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
    glm::vec3 cameraPos(55.0f, -55.0f, 500.0f);
    int shown = 0;
    int exitCode = 0;

    while (!glfwWindowShouldClose(window)) {
        processInput(window);
        trackSliderDrag();
        if (dropFrame >= 0 && shown == dropFrame) {
            add_disturbance(dropGridX, dropGridY, dropAmplitude);
            dropFrame = -1;
        }
        apply_base_disturbance(shown);

        update_wave();
        uploadHeightTexture();

        glClearColor(0.45f, 0.65f, 0.85f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        beautyView = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, -30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
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
        float meshSpacing = ((width - 1) * waterScale) / (axisX - 1);
        glUniform1f(glGetUniformLocation(causticsShaderProgram, "meshSpacing"), meshSpacing);
        glUniform1i(glGetUniformLocation(causticsShaderProgram, "heightTex"), 1);

        glBindVertexArray(waterVAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)waterIndices.size(), GL_UNSIGNED_INT, 0);
        blurCausticMap();

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

        drawSliders();

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
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                dumpFrameCount = std::atoi(argv[++i]);
            }
        } else if (std::strcmp(argv[i], "--wind") == 0 && i + 3 < argc) {
            baseDisturbance.amplitude = (float)std::atof(argv[++i]);
            baseDisturbance.wavelength = (float)std::atof(argv[++i]);
            baseDisturbance.tempo = (float)std::atof(argv[++i]);
            if (!(baseDisturbance.wavelength > 0.0f)) {
                baseDisturbance.wavelength = 40.0f;
            }
        } else if (std::strcmp(argv[i], "--drop") == 0 && i + 4 < argc) {
            dropGridX = std::atoi(argv[++i]);
            dropGridY = std::atoi(argv[++i]);
            dropAmplitude = (float)std::atof(argv[++i]);
            dropFrame = std::atoi(argv[++i]);
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
    causticBlurShaderProgram = createShaderProgram(causticBlurVertexShaderSource, causticBlurFragmentShaderSource);
    bottomShaderProgram = createShaderProgram(bottomVertexShaderSource, bottomFragmentShaderSource);
    overlayShaderProgram = createShaderProgram(overlayVertexShaderSource, overlayFragmentShaderSource);
    overlayColorLoc = glGetUniformLocation(overlayShaderProgram, "color");
    overlayAlphaLoc = glGetUniformLocation(overlayShaderProgram, "alpha");
    overlayViewportLoc = glGetUniformLocation(overlayShaderProgram, "viewport");

    init_grid();

    buildWaterMesh();
    generateBottomMesh();
    generateSkyboxMesh();
    initOverlayMesh();

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
    glDeleteFramebuffers(1, &causticsBlurFBO);
    glDeleteTextures(1, &causticsBlurTexture);
    glDeleteVertexArrays(1, &causticBlurVAO);
    glDeleteTextures(1, &heightTexture);
    glDeleteProgram(waterShaderProgram);
    glDeleteProgram(skyboxShaderProgram);
    glDeleteProgram(causticsShaderProgram);
    glDeleteProgram(causticBlurShaderProgram);
    glDeleteProgram(bottomShaderProgram);
    glDeleteVertexArrays(1, &overlayVAO);
    glDeleteBuffers(1, &overlayVBO);
    glDeleteProgram(overlayShaderProgram);

    glfwTerminate();
    return exitCode;
}
