#include "app.h"

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
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

std::string toLowerCopy(const char* text) {
    std::string out;
    if (text == nullptr) {
        return out;
    }
    for (std::size_t i = 0; text[i] != '\0'; ++i) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(text[i]))));
    }
    return out;
}

} // namespace

App::~App() {
    shutdown();
}

const char* App::boundaryModeLabel(BoundaryMode mode) {
    switch (mode) {
        case BoundaryMode::ReflectiveNeumann:
            return "NEUMANN";
        case BoundaryMode::PartialAbsorbRobin:
            return "ROBIN";
        case BoundaryMode::AbsorbingOpen:
            return "ABSORB";
        case BoundaryMode::FixedDirichlet:
            return "DIRICHLET";
    }
    return "DIRICHLET";
}

void App::cycleBoundaryMode() {
    switch (waveSim_.boundaryMode) {
        case BoundaryMode::ReflectiveNeumann:
            waveSim_.boundaryMode = BoundaryMode::PartialAbsorbRobin;
            break;
        case BoundaryMode::PartialAbsorbRobin:
            waveSim_.boundaryMode = BoundaryMode::AbsorbingOpen;
            break;
        case BoundaryMode::AbsorbingOpen:
            waveSim_.boundaryMode = BoundaryMode::FixedDirichlet;
            break;
        case BoundaryMode::FixedDirichlet:
            waveSim_.boundaryMode = BoundaryMode::ReflectiveNeumann;
            break;
    }
}

void App::syncSphericalFromLight() {
    const float radius = glm::length(light_.position);
    lightRadius_ = (radius > 10.0f) ? radius : 100.0f;
    const float horiz = std::hypot(light_.position.x, light_.position.y);
    lightElevationDeg_ = std::clamp(glm::degrees(std::atan2(light_.position.z, horiz)), 20.0f, 88.0f);
    lightAzimuthDeg_ = (horiz > 1e-4f) ? glm::degrees(std::atan2(light_.position.y, light_.position.x)) : 0.0f;
}

void App::syncLightFromSpherical() {
    lightElevationDeg_ = std::clamp(lightElevationDeg_, 20.0f, 88.0f);
    while (lightAzimuthDeg_ > 180.0f) {
        lightAzimuthDeg_ -= 360.0f;
    }
    while (lightAzimuthDeg_ < -180.0f) {
        lightAzimuthDeg_ += 360.0f;
    }
    const float elRad = glm::radians(lightElevationDeg_);
    const float azRad = glm::radians(lightAzimuthDeg_);
    const float cosEl = std::cos(elRad);
    light_.position = glm::vec3(
        lightRadius_ * cosEl * std::cos(azRad),
        lightRadius_ * cosEl * std::sin(azRad),
        lightRadius_ * std::sin(elRad)
    );
}

bool App::parseArgs(int argc, char** argv) {
    syncSphericalFromLight();

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
        } else if (std::strcmp(argv[i], "--camera") == 0 && i + 6 < argc) {
            const float ex = static_cast<float>(std::atof(argv[++i]));
            const float ey = static_cast<float>(std::atof(argv[++i]));
            const float ez = static_cast<float>(std::atof(argv[++i]));
            const float tx = static_cast<float>(std::atof(argv[++i]));
            const float ty = static_cast<float>(std::atof(argv[++i]));
            const float tz = static_cast<float>(std::atof(argv[++i]));
            camera_.setPositionTargetAutoUp(glm::vec3(ex, ey, ez), glm::vec3(tx, ty, tz));
        } else if (std::strcmp(argv[i], "--preset") == 0 && i + 1 < argc) {
            const int presetId = std::atoi(argv[++i]);
            switch (presetId) {
                case 1:
                    camera_.setPreset(CameraPreset::TopDown);
                    break;
                case 2:
                    camera_.setPreset(CameraPreset::Perspective);
                    break;
                case 3:
                    camera_.setPreset(CameraPreset::LowAngle);
                    break;
                case 4:
                    camera_.setPreset(CameraPreset::Side);
                    break;
                default:
                    std::cerr << "Invalid --preset value: " << presetId << " (expected 1..4)\n";
                    return false;
            }
        } else if (std::strcmp(argv[i], "--light") == 0 && i + 3 < argc) {
            const float lx = static_cast<float>(std::atof(argv[++i]));
            const float ly = static_cast<float>(std::atof(argv[++i]));
            const float lz = static_cast<float>(std::atof(argv[++i]));
            light_.position = glm::vec3(lx, ly, lz);
            syncSphericalFromLight();
        } else if (std::strcmp(argv[i], "--boundary") == 0 && i + 1 < argc) {
            const std::string mode = toLowerCopy(argv[++i]);
            if (mode == "reflective" || mode == "neumann") {
                waveSim_.boundaryMode = BoundaryMode::ReflectiveNeumann;
            } else if (mode == "robin") {
                waveSim_.boundaryMode = BoundaryMode::PartialAbsorbRobin;
            } else if (mode == "absorb" || mode == "absorbing") {
                waveSim_.boundaryMode = BoundaryMode::AbsorbingOpen;
            } else if (mode == "dirichlet") {
                waveSim_.boundaryMode = BoundaryMode::FixedDirichlet;
            } else {
                std::cerr << "Invalid --boundary mode: " << mode
                          << " (expected reflective|robin|absorb|dirichlet)\n";
                return false;
            }
        } else if (std::strcmp(argv[i], "--dispersion") == 0 && i + 1 < argc) {
            settings_.dispersion = std::max(0.0f, static_cast<float>(std::atof(argv[++i])));
        } else if (std::strcmp(argv[i], "--no-ui") == 0) {
            settings_.showUI = false;
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
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    window_ = glfwCreateWindow(static_cast<int>(kWindowWidth), static_cast<int>(kWindowHeight), "Water Caustics", nullptr, nullptr);
    if (window_ == nullptr) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferSizeCallback);
    glfwSetWindowSizeCallback(window_, windowSizeCallback);
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    glfwSetCursorPosCallback(window_, cursorPosCallback);
    glfwSetScrollCallback(window_, scrollCallback);
    glfwSetKeyCallback(window_, keyCallback);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
        return false;
    }

    glfwSwapInterval(dumpPath_ != nullptr ? 0 : 1);

    glfwGetFramebufferSize(window_, &framebufferWidth_, &framebufferHeight_);
    glfwGetWindowSize(window_, &windowWidth_, &windowHeight_);
    if (windowWidth_ <= 0 || windowHeight_ <= 0) {
        windowWidth_ = static_cast<int>(kWindowWidth);
        windowHeight_ = static_cast<int>(kWindowHeight);
    }
    if (framebufferWidth_ > 0 && framebufferHeight_ > 0) {
        glViewport(0, 0, framebufferWidth_, framebufferHeight_);
    }
    return true;
}

bool App::ensureSceneTargets(int width, int height) {
    if (width <= 0 || height <= 0) {
        return false;
    }
    if (sceneTargets_.valid() && sceneTargets_.width == width && sceneTargets_.height == height) {
        return true;
    }

    destroySceneTargets();

    glGenFramebuffers(1, &sceneTargets_.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneTargets_.fbo);

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &sceneTargets_.colorTex);
    glBindTexture(GL_TEXTURE_2D, sceneTargets_.colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTargets_.colorTex, 0);

    glGenTextures(1, &sceneTargets_.depthTex);
    glBindTexture(GL_TEXTURE_2D, sceneTargets_.depthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sceneTargets_.depthTex, 0);

    GLenum drawBuf = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &drawBuf);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        // Fallback to RGBA8 if RGBA16F is unsupported as a render target
        glBindTexture(GL_TEXTURE_2D, sceneTargets_.colorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTargets_.colorTex, 0);
    }

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "SceneTargets FBO incomplete (" << status << ")\n";
        destroySceneTargets();
        return false;
    }

    sceneTargets_.width = width;
    sceneTargets_.height = height;
    return true;
}

void App::destroySceneTargets() {
    if (sceneTargets_.fbo != 0) {
        glDeleteFramebuffers(1, &sceneTargets_.fbo);
        sceneTargets_.fbo = 0;
    }
    if (sceneTargets_.colorTex != 0) {
        glDeleteTextures(1, &sceneTargets_.colorTex);
        sceneTargets_.colorTex = 0;
    }
    if (sceneTargets_.depthTex != 0) {
        glDeleteTextures(1, &sceneTargets_.depthTex);
        sceneTargets_.depthTex = 0;
    }
    sceneTargets_.width = 0;
    sceneTargets_.height = 0;
}

void App::setupHudControls() {
    overlayUI_.clearSliders();
    overlayUI_.clearButtons();

    overlayUI_.addSlider({"AMP", 0.0f, 0.04f, &wind_.amplitude, "%.3f", nullptr});
    overlayUI_.addSlider({"SCALE", 16.0f, 80.0f, &wind_.wavelength, "%.1f", nullptr});
    overlayUI_.addSlider({"TEMPO", 0.0f, 0.20f, &wind_.tempo, "%.3f", nullptr});
    overlayUI_.addSlider({"REFLECT", 0.0f, 1.0f, &waveSim_.wallReflection, "%.2f", nullptr});
    overlayUI_.addSlider({"LIGHT EL", 20.0f, 88.0f, &lightElevationDeg_, "%.1f",
                          [this]() { syncLightFromSpherical(); }});
    overlayUI_.addSlider({"LIGHT AZ", -180.0f, 180.0f, &lightAzimuthDeg_, "%.1f",
                          [this]() { syncLightFromSpherical(); }});
    overlayUI_.addSlider({"DISP", 0.0f, 0.06f, &settings_.dispersion, "%.3f", nullptr});

    overlayUI_.addButton({
        "BOUNDARY",
        [this]() { return boundaryModeLabel(waveSim_.boundaryMode); },
        [this]() { cycleBoundaryMode(); }
    });
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
    if (!overlayUI_.init(static_cast<float>(windowWidth_), static_cast<float>(windowHeight_))) {
        shutdown();
        return false;
    }

    setupHudControls();

    if (framebufferWidth_ > 0 && framebufferHeight_ > 0) {
        ensureSceneTargets(framebufferWidth_, framebufferHeight_);
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    needHeightUpload_ = true;
    return true;
}

void App::shutdown() {
    if (window_ != nullptr) {
        destroySceneTargets();
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

void App::windowSizeCallback(GLFWwindow* win, int winW, int winH) {
    App* self = static_cast<App*>(glfwGetWindowUserPointer(win));
    if (self != nullptr) {
        self->onWindowSize(winW, winH);
    }
}

void App::mouseButtonCallback(GLFWwindow* win, int button, int action, int mods) {
    App* self = static_cast<App*>(glfwGetWindowUserPointer(win));
    if (self != nullptr) {
        self->onMouseButton(button, action, mods);
    }
}

void App::cursorPosCallback(GLFWwindow* win, double xpos, double ypos) {
    App* self = static_cast<App*>(glfwGetWindowUserPointer(win));
    if (self != nullptr) {
        self->onCursorPos(xpos, ypos);
    }
}

void App::scrollCallback(GLFWwindow* win, double xoffset, double yoffset) {
    App* self = static_cast<App*>(glfwGetWindowUserPointer(win));
    if (self != nullptr) {
        self->onScroll(xoffset, yoffset);
    }
}

void App::keyCallback(GLFWwindow* win, int key, int scancode, int action, int mods) {
    App* self = static_cast<App*>(glfwGetWindowUserPointer(win));
    if (self != nullptr) {
        self->onKey(key, scancode, action, mods);
    }
}

void App::onFramebufferSize(int fbW, int fbH) {
    framebufferWidth_ = fbW;
    framebufferHeight_ = fbH;
    if (fbW > 0 && fbH > 0) {
        glViewport(0, 0, fbW, fbH);
        ensureSceneTargets(fbW, fbH);
    }
}

void App::onWindowSize(int winW, int winH) {
    windowWidth_ = winW;
    windowHeight_ = winH;
    if (winW > 0 && winH > 0) {
        overlayUI_.setViewportSize(static_cast<float>(winW), static_cast<float>(winH));
    }
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
    fbX = static_cast<float>(xpos) * static_cast<float>(fbW) / static_cast<float>(winW);
    fbY = static_cast<float>(fbH) - static_cast<float>(ypos) * static_cast<float>(fbH) / static_cast<float>(winH);
    viewX = static_cast<float>(xpos);
    viewY = static_cast<float>(winH) - static_cast<float>(ypos);
    outFbW = fbW;
    outFbH = fbH;
    return true;
}

void App::onMouseButton(int button, int action, int /*mods*/) {
    glfwGetCursorPos(window_, &lastCursorX_, &lastCursorY_);

    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        rightButtonDown_ = (action == GLFW_PRESS);
        return;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        middleButtonDown_ = (action == GLFW_PRESS);
        return;
    }
    if (button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    if (action == GLFW_RELEASE) {
        leftButtonDown_ = false;
        lightDragActive_ = false;
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

    int winW = 0;
    int winH = 0;
    glfwGetWindowSize(window_, &winW, &winH);
    if (winW > 0 && winH > 0) {
        windowWidth_ = winW;
        windowHeight_ = winH;
        overlayUI_.setViewportSize(static_cast<float>(winW), static_cast<float>(winH));
    }

    if (settings_.showUI && overlayUI_.onMouseDown(viewX, viewY)) {
        return;
    }

    if (glfwGetKey(window_, GLFW_KEY_L) == GLFW_PRESS) {
        lightDragActive_ = true;
        return;
    }

    const Ray ray = camera_.unprojectRay(fbX, fbY, fbW, fbH);
    if (std::abs(ray.direction.z) < 1e-6f) {
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
        needHeightUpload_ = true;
    }
}

void App::onCursorPos(double xpos, double ypos) {
    const double dx = xpos - lastCursorX_;
    const double dy = ypos - lastCursorY_;
    lastCursorX_ = xpos;
    lastCursorY_ = ypos;

    if (leftButtonDown_ && overlayUI_.isDragging()) {
        int winW = 0;
        int winH = 0;
        glfwGetWindowSize(window_, &winW, &winH);
        if (winW > 0 && winH > 0) {
            overlayUI_.setViewportSize(static_cast<float>(winW), static_cast<float>(winH));
            overlayUI_.onMouseDrag(static_cast<float>(xpos), static_cast<float>(winH) - static_cast<float>(ypos));
        }
        return;
    }

    if (lightDragActive_) {
        lightAzimuthDeg_ += static_cast<float>(dx) * 0.45f;
        lightElevationDeg_ -= static_cast<float>(dy) * 0.35f;
        syncLightFromSpherical();
        return;
    }

    const bool shiftHeld = (glfwGetKey(window_, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ||
                           (glfwGetKey(window_, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

    if (middleButtonDown_ || (rightButtonDown_ && shiftHeld)) {
        const float panScale = std::max(0.08f, camera_.distance() * 0.0018f);
        camera_.pan(-static_cast<float>(dx) * panScale, static_cast<float>(dy) * panScale);
    } else if (rightButtonDown_) {
        camera_.orbit(-static_cast<float>(dx) * 0.006f, static_cast<float>(dy) * 0.006f);
    }
}

void App::onScroll(double /*xoffset*/, double yoffset) {
    const float zoomStep = std::max(8.0f, camera_.distance() * 0.08f);
    camera_.zoom(-static_cast<float>(yoffset) * zoomStep);
}

void App::onKey(int key, int /*scancode*/, int action, int /*mods*/) {
    if (action != GLFW_PRESS) {
        return;
    }
    switch (key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
            break;
        case GLFW_KEY_1:
            camera_.setPreset(CameraPreset::TopDown);
            break;
        case GLFW_KEY_2:
            camera_.setPreset(CameraPreset::Perspective);
            break;
        case GLFW_KEY_3:
            camera_.setPreset(CameraPreset::LowAngle);
            break;
        case GLFW_KEY_4:
            camera_.setPreset(CameraPreset::Side);
            break;
        case GLFW_KEY_SPACE:
            paused_ = !paused_;
            break;
        case GLFW_KEY_R:
            waveSim_.reset();
            needHeightUpload_ = true;
            break;
        case GLFW_KEY_B:
            cycleBoundaryMode();
            break;
        case GLFW_KEY_H:
            settings_.showUI = !settings_.showUI;
            break;
        default:
            break;
    }
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
    const bool dumpMode = (dumpPath_ != nullptr);
    int shown = 0;
    int simStep = 0;
    int exitCode = 0;

    constexpr double kFixedStepSec = 1.0 / 60.0;
    constexpr double kMaxFrameDtSec = 0.05;
    double lastTimeSec = glfwGetTime();
    double simAccumulatorSec = 0.0;

    while (!glfwWindowShouldClose(window_)) {
        if (dumpMode) {
            if (dropFrame_ >= 0 && shown == dropFrame_) {
                waveSim_.addDisturbance(dropGridX_, dropGridY_, dropAmplitude_);
                dropFrame_ = -1;
            }
            waveSim_.applyWind(static_cast<float>(shown), wind_);
            waveSim_.step();
            causticsPass_.uploadHeight(waveSim_);
            needHeightUpload_ = false;
        } else {
            const double nowSec = glfwGetTime();
            const double frameDt = std::min(std::max(nowSec - lastTimeSec, 0.0), kMaxFrameDtSec);
            lastTimeSec = nowSec;

            bool stepped = false;
            if (!paused_) {
                simAccumulatorSec += frameDt;
                while (simAccumulatorSec >= kFixedStepSec) {
                    if (dropFrame_ >= 0 && simStep == dropFrame_) {
                        waveSim_.addDisturbance(dropGridX_, dropGridY_, dropAmplitude_);
                        dropFrame_ = -1;
                    }
                    waveSim_.applyWind(static_cast<float>(simStep), wind_);
                    waveSim_.step();
                    ++simStep;
                    simAccumulatorSec -= kFixedStepSec;
                    stepped = true;
                }
            }
            if (stepped || needHeightUpload_) {
                causticsPass_.uploadHeight(waveSim_);
                needHeightUpload_ = false;
            }
        }

        glfwGetFramebufferSize(window_, &framebufferWidth_, &framebufferHeight_);
        glfwGetWindowSize(window_, &windowWidth_, &windowHeight_);

        if (framebufferWidth_ <= 0 || framebufferHeight_ <= 0) {
            glfwPollEvents();
            if (dumpMode && shown + 1 >= dumpFrameCount_) {
                exitCode = 1;
                break;
            }
            continue;
        }

        ensureSceneTargets(framebufferWidth_, framebufferHeight_);

        const float aspect = static_cast<float>(framebufferWidth_) / static_cast<float>(framebufferHeight_);
        const CameraState camState = camera_.state(aspect);

        causticsPass_.render(waterRenderer_.vao(),
                             waterRenderer_.indexCount(),
                             waterRenderer_.meshSpacing(),
                             pool_,
                             light_,
                             settings_);

        const bool useSceneTargets = sceneTargets_.valid();
        if (useSceneTargets) {
            glBindFramebuffer(GL_FRAMEBUFFER, sceneTargets_.fbo);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        glViewport(0, 0, framebufferWidth_, framebufferHeight_);
        glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glClearColor(0.45f, 0.65f, 0.85f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);
        skyboxRenderer_.render(camState, light_);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);

        poolRenderer_.render(camState,
                             pool_,
                             light_,
                             settings_,
                             causticsPass_.causticsTexture(),
                             causticsPass_.heightTexture());

        if (useSceneTargets) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneTargets_.fbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(0, 0, framebufferWidth_, framebufferHeight_,
                              0, 0, framebufferWidth_, framebufferHeight_,
                              GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, framebufferWidth_, framebufferHeight_);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);

        waterRenderer_.render(camState,
                              pool_,
                              light_,
                              settings_,
                              causticsPass_.heightTexture(),
                              useSceneTargets ? &sceneTargets_ : nullptr);

        if (settings_.showUI && windowWidth_ > 0 && windowHeight_ > 0) {
            overlayUI_.render(static_cast<float>(windowWidth_), static_cast<float>(windowHeight_));
        }

        const bool dumpNow = dumpMode && shown + 1 >= dumpFrameCount_;
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
