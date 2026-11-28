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
    void processInput();
    bool readCursor(float& fbX, float& fbY, float& viewX, float& viewY, int& outFbW, int& outFbH) const;
    void trackSliderDrag();
    bool captureWindowBmp(const char* path) const;

    void onFramebufferSize(int fbW, int fbH);
    void onMouseButton(int button, int action, int mods);

    static void framebufferSizeCallback(GLFWwindow* win, int fbW, int fbH);
    static void mouseButtonCallback(GLFWwindow* win, int button, int action, int mods);

    GLFWwindow* window_ = nullptr;
    int framebufferWidth_ = static_cast<int>(kWindowWidth);
    int framebufferHeight_ = static_cast<int>(kWindowHeight);

    PoolConfig pool_;
    LightConfig light_;
    RenderSettings settings_;
    WindParams wind_;
    WaveSim waveSim_;
    Camera camera_;

    CausticsPass causticsPass_;
    PoolRenderer poolRenderer_;
    WaterRenderer waterRenderer_;
    SkyboxRenderer skyboxRenderer_;
    OverlayUI overlayUI_;

    bool leftButtonDown_ = false;
    const char* dumpPath_ = nullptr;
    int dumpFrameCount_ = -1;
    int dropGridX_ = 0;
    int dropGridY_ = 0;
    float dropAmplitude_ = 0.0f;
    int dropFrame_ = -1;
};
