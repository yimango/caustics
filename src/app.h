#pragma once

#include "render/caustics_pass.h"
#include "render/pool_coords.h"
#include "render/pool_renderer.h"
#include "render/scene_state.h"
#include "render/skybox_renderer.h"
#include "render/water_renderer.h"
#include "sim/wave_sim.h"
#include "ui/camera.h"
#include "ui/overlay_ui.h"

struct GLFWwindow;

class App {
public:
    static constexpr unsigned int kWindowWidth = 1600;
    static constexpr unsigned int kWindowHeight = 900;

    ~App();

    bool parseArgs(int argc, char** argv);
    bool init();
    int run();
    void shutdown();

private:
    bool initGL();
    bool ensureSceneTargets(int width, int height);
    void destroySceneTargets();
    void setupHudControls();
    void syncSphericalFromLight();
    void syncLightFromSpherical();
    void cycleBoundaryMode();
    static const char* boundaryModeLabel(BoundaryMode mode);

    bool readCursor(float& fbX, float& fbY, float& viewX, float& viewY, int& outFbW, int& outFbH) const;
    bool captureWindowBmp(const char* path) const;

    void onFramebufferSize(int fbW, int fbH);
    void onWindowSize(int winW, int winH);
    void onMouseButton(int button, int action, int mods);
    void onCursorPos(double xpos, double ypos);
    void onScroll(double xoffset, double yoffset);
    void onKey(int key, int scancode, int action, int mods);

    static void framebufferSizeCallback(GLFWwindow* win, int fbW, int fbH);
    static void windowSizeCallback(GLFWwindow* win, int winW, int winH);
    static void mouseButtonCallback(GLFWwindow* win, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* win, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* win, double xoffset, double yoffset);
    static void keyCallback(GLFWwindow* win, int key, int scancode, int action, int mods);

    GLFWwindow* window_ = nullptr;
    int framebufferWidth_ = static_cast<int>(kWindowWidth);
    int framebufferHeight_ = static_cast<int>(kWindowHeight);
    int windowWidth_ = static_cast<int>(kWindowWidth);
    int windowHeight_ = static_cast<int>(kWindowHeight);

    PoolConfig pool_;
    LightConfig light_;
    RenderSettings settings_;
    WindParams wind_;
    WaveSim waveSim_;
    Camera camera_;
    SceneTargets sceneTargets_;

    CausticsPass causticsPass_;
    PoolRenderer poolRenderer_;
    WaterRenderer waterRenderer_;
    SkyboxRenderer skyboxRenderer_;
    OverlayUI overlayUI_;

    float lightElevationDeg_ = 88.0f;
    float lightAzimuthDeg_ = 0.0f;
    float lightRadius_ = 100.0f;

    bool leftButtonDown_ = false;
    bool rightButtonDown_ = false;
    bool middleButtonDown_ = false;
    bool lightDragActive_ = false;
    double lastCursorX_ = 0.0;
    double lastCursorY_ = 0.0;

    bool paused_ = false;
    bool needHeightUpload_ = true;

    const char* dumpPath_ = nullptr;
    int dumpFrameCount_ = -1;
    int dropGridX_ = 0;
    int dropGridY_ = 0;
    float dropAmplitude_ = 0.0f;
    int dropFrame_ = -1;
};
