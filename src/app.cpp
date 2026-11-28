#include "app.h"

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

void storeU16(unsigned char* dst, unsigned int value) {
    dst[0] = static_cast<unsigned char>(value & 255u);
    dst[1] = static_cast<unsigned char>((value >> 8) & 255u);
}

void storeU32(unsigned char* dst, unsigned int value) {
    dst[0] = static_cast<unsigned char>(value & 255u);
    dst[1] = static_cast<unsigned char>((value >> 8) & 255u);
    dst[2] = static_cast<unsigned char>((value >> 16) & 255u);
    dst[3] = static_cast<unsigned char>((value >> 24) & 255u);
}

} // namespace

App::~App() {
    shutdown();
}

bool App::parseArgs(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--dump") == 0 && i + 1 < argc) {
            dumpPath_ = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                dumpFrameCount_ = std::atoi(argv[++i]);
            }
        } else if (std::strcmp(argv[i], "--wind") == 0 && i + 3 < argc) {
            wind_.amplitude = static_cast<float>(std::atof(argv[++i]));
            wind_.wavelength = static_cast<float>(std::atof(argv[++i]));
            wind_.tempo = static_cast<float>(std::atof(argv[++i]));
            if (!(wind_.wavelength > 0.0f)) {
                wind_.wavelength = 40.0f;
            }
        } else if (std::strcmp(argv[i], "--drop") == 0 && i + 4 < argc) {
            dropGridX_ = std::atoi(argv[++i]);
            dropGridY_ = std::atoi(argv[++i]);
            dropAmplitude_ = static_cast<float>(std::atof(argv[++i]));
            dropFrame_ = std::atoi(argv[++i]);
        }
    }
    if (dumpPath_ != nullptr && dumpFrameCount_ < 0) {
        dumpFrameCount_ = 180;
    }
    return true;
}

bool App::initGL() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window_ = glfwCreateWindow(static_cast<int>(kWindowWidth), static_cast<int>(kWindowHeight), "Water Caustics", nullptr, nullptr);
    if (window_ == nullptr) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferSizeCallback);
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
        return false;
    }

    glfwGetFramebufferSize(window_, &framebufferWidth_, &framebufferHeight_);
    if (framebufferWidth_ <= 0 || framebufferHeight_ <= 0) {
        framebufferWidth_ = static_cast<int>(kWindowWidth);
        framebufferHeight_ = static_cast<int>(kWindowHeight);
    }
    glViewport(0, 0, framebufferWidth_, framebufferHeight_);
    return true;
}

bool App::init() {
    if (!initGL()) {
        return false;
    }

    waveSim_.init(pool_.gridW, pool_.gridH);

    if (!waterRenderer_.init(pool_)) {
        shutdown();
        return false;
    }
    if (!poolRenderer_.init(pool_)) {
        shutdown();
        return false;
    }
    if (!skyboxRenderer_.init()) {
        shutdown();
        return false;
    }
    if (!causticsPass_.init(pool_)) {
        shutdown();
        return false;
    }
    if (!overlayUI_.init(static_cast<float>(kWindowWidth), static_cast<float>(kWindowHeight))) {
        shutdown();
        return false;
    }

    overlayUI_.clearSliders();
    overlayUI_.addSlider({"AMP", 0.0f, 0.04f, &wind_.amplitude, "%.3f"});
    overlayUI_.addSlider({"SCALE", 16.0f, 80.0f, &wind_.wavelength, "%.1f"});
    overlayUI_.addSlider({"TEMPO", 0.0f, 0.20f, &wind_.tempo, "%.3f"});

    glEnable(GL_DEPTH_TEST);
    return true;
}

void App::shutdown() {
    if (window_ != nullptr) {
        overlayUI_.destroy();
        causticsPass_.destroy();
        skyboxRenderer_.destroy();
        poolRenderer_.destroy();
        waterRenderer_.destroy();
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
    }
}

void App::framebufferSizeCallback(GLFWwindow* win, int fbW, int fbH) {
    App* self = static_cast<App*>(glfwGetWindowUserPointer(win));
    if (self != nullptr) {
        self->onFramebufferSize(fbW, fbH);
    }
}

void App::mouseButtonCallback(GLFWwindow* win, int button, int action, int mods) {
    App* self = static_cast<App*>(glfwGetWindowUserPointer(win));
    if (self != nullptr) {
        self->onMouseButton(button, action, mods);
    }
}

void App::onFramebufferSize(int fbW, int fbH) {
    framebufferWidth_ = fbW;
    framebufferHeight_ = fbH;
    glViewport(0, 0, fbW, fbH);
}

bool App::readCursor(float& fbX, float& fbY, float& viewX, float& viewY, int& outFbW, int& outFbH) const {
    double xpos = 0.0;
    double ypos = 0.0;
    glfwGetCursorPos(window_, &xpos, &ypos);
    int fbW = 0;
    int fbH = 0;
    glfwGetFramebufferSize(window_, &fbW, &fbH);
    int winW = 0;
    int winH = 0;
    glfwGetWindowSize(window_, &winW, &winH);
    if (fbW <= 0 || fbH <= 0 || winW <= 0 || winH <= 0) {
        return false;
    }
    const float cursorX = static_cast<float>(xpos) * static_cast<float>(fbW) / static_cast<float>(winW);
    const float cursorY = static_cast<float>(fbH) - static_cast<float>(ypos) * static_cast<float>(fbH) / static_cast<float>(winH);
    fbX = cursorX;
    fbY = cursorY;
    viewX = cursorX * overlayUI_.virtualWidth() / static_cast<float>(fbW);
    viewY = cursorY * overlayUI_.virtualHeight() / static_cast<float>(fbH);
    outFbW = fbW;
    outFbH = fbH;
    return true;
}

void App::onMouseButton(int button, int action, int /*mods*/) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    if (action == GLFW_RELEASE) {
        leftButtonDown_ = false;
        overlayUI_.onMouseUp();
        return;
    }
    if (action != GLFW_PRESS) {
        return;
    }
    leftButtonDown_ = true;

    float fbX = 0.0f;
    float fbY = 0.0f;
    float viewX = 0.0f;
    float viewY = 0.0f;
    int fbW = 0;
    int fbH = 0;
    if (!readCursor(fbX, fbY, viewX, viewY, fbW, fbH)) {
        return;
    }
    if (settings_.showUI && overlayUI_.onMouseDown(viewX, viewY)) {
        return;
    }

    const Ray ray = camera_.unprojectRay(fbX, fbY, fbW, fbH);
    if (ray.direction.z == 0.0f) {
        return;
    }
    const float travel = -ray.origin.z / ray.direction.z;
    if (travel < 0.0f) {
        return;
    }
    const glm::vec3 hit = ray.origin + ray.direction * travel;
    const int gridX = static_cast<int>(std::lround(pool_.worldToGridX(hit.x)));
    const int gridY = static_cast<int>(std::lround(pool_.worldToGridY(hit.y)));
    if (gridX >= 0 && gridX < waveSim_.width && gridY >= 0 && gridY < waveSim_.height) {
        waveSim_.addDisturbance(gridX, gridY, 5.0f);
    }
}

void App::processInput() {
    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }
}

void App::trackSliderDrag() {
    if (!leftButtonDown_ || !overlayUI_.isDragging()) {
        return;
    }
    float fbX = 0.0f;
    float fbY = 0.0f;
    float viewX = 0.0f;
    float viewY = 0.0f;
    int fbW = 0;
    int fbH = 0;
    if (!readCursor(fbX, fbY, viewX, viewY, fbW, fbH)) {
        return;
    }
    overlayUI_.onMouseDrag(viewX, viewY);
}

bool App::captureWindowBmp(const char* path) const {
    if (path == nullptr || framebufferWidth_ <= 0 || framebufferHeight_ <= 0) {
        std::cerr << "Window framebuffer is empty\n";
        return false;
    }

    const int fbW = framebufferWidth_;
    const int fbH = framebufferHeight_;
    std::vector<unsigned char> rgb(static_cast<std::size_t>(fbW) * static_cast<std::size_t>(fbH) * 3);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, fbW, fbH, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    glPixelStorei(GL_PACK_ALIGNMENT, 4);

    const int rowStride = (fbW * 3 + 3) & ~3;
    const int pixelBytes = rowStride * fbH;
    const int fileSize = 54 + pixelBytes;
    std::vector<unsigned char> file(static_cast<std::size_t>(fileSize), 0);
    file[0] = 'B';
    file[1] = 'M';
    storeU32(file.data() + 2, static_cast<unsigned int>(fileSize));
    storeU32(file.data() + 10, 54);
    storeU32(file.data() + 14, 40);
    storeU32(file.data() + 18, static_cast<unsigned int>(fbW));
    storeU32(file.data() + 22, static_cast<unsigned int>(fbH));
    storeU16(file.data() + 26, 1);
    storeU16(file.data() + 28, 24);
    storeU32(file.data() + 34, static_cast<unsigned int>(pixelBytes));

    for (int y = 0; y < fbH; ++y) {
        unsigned char* dst = file.data() + 54 + static_cast<std::size_t>(y) * static_cast<std::size_t>(rowStride);
        const unsigned char* src = rgb.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(fbW) * 3;
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
    out.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
    return static_cast<bool>(out);
}

int App::run() {
    int shown = 0;
    int exitCode = 0;

    while (!glfwWindowShouldClose(window_)) {
        processInput();
        trackSliderDrag();

        if (dropFrame_ >= 0 && shown == dropFrame_) {
            waveSim_.addDisturbance(dropGridX_, dropGridY_, dropAmplitude_);
            dropFrame_ = -1;
        }
        waveSim_.applyWind(static_cast<float>(shown), wind_);
        waveSim_.step();
        causticsPass_.uploadHeight(waveSim_);

        const float aspect = (framebufferHeight_ > 0)
            ? static_cast<float>(framebufferWidth_) / static_cast<float>(framebufferHeight_)
            : static_cast<float>(kWindowWidth) / static_cast<float>(kWindowHeight);
        const CameraState camState = camera_.state(aspect);

        causticsPass_.render(waterRenderer_.vao(),
                             waterRenderer_.indexCount(),
                             waterRenderer_.meshSpacing(),
                             pool_,
                             light_,
                             settings_);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, framebufferWidth_, framebufferHeight_);
        glClearColor(0.45f, 0.65f, 0.85f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        skyboxRenderer_.render(camState, light_);
        poolRenderer_.render(camState, pool_, light_, settings_, causticsPass_.causticsTexture());
        waterRenderer_.render(camState, pool_, light_, settings_, causticsPass_.heightTexture(), nullptr);

        if (settings_.showUI) {
            overlayUI_.render();
        }

        const bool dumpNow = dumpPath_ != nullptr && shown + 1 >= dumpFrameCount_;
        if (dumpNow && !captureWindowBmp(dumpPath_)) {
            exitCode = 1;
        }

        glfwSwapBuffers(window_);
        glfwPollEvents();
        ++shown;
        if (dumpNow) {
            break;
        }
    }

    return exitCode;
}
