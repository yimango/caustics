#pragma once

#include <vector>
#include "render/shader_program.h"

struct HudSlider {
    const char* label = "";
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float* value = nullptr;
    const char* format = "%.3f";
};

class OverlayUI {
public:
    ~OverlayUI();

    bool init(float virtualWidth = 1600.0f, float virtualHeight = 900.0f);
    void destroy();

    void clearSliders();
    void addSlider(const HudSlider& slider);

    bool onMouseDown(float viewX, float viewY);
    void onMouseDrag(float viewX, float viewY);
    void onMouseUp();
    bool isDragging() const { return activeSlider_ >= 0; }

    void render() const;

    float virtualWidth() const { return virtualW_; }
    float virtualHeight() const { return virtualH_; }

private:
    float panelTopY() const;
    float panelHeight() const;
    float panelBottomY() const;
    float rowBottomY(int index) const;
    float sliderTrackX() const;
    float sliderValueX() const;
    float sliderTrackW() const;
    int sliderRowAt(float x, float y) const;
    void writeSliderAt(int index, float cursorX);

    void drawRect(float x, float y, float w, float h, float r, float g, float b, float a) const;
    void drawGlyph(char code, float x, float y, float scale, float r, float g, float b) const;
    void drawText(const char* text, float x, float y, float scale, float r, float g, float b) const;

    ShaderProgram shader_;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    std::vector<HudSlider> sliders_;
    int activeSlider_ = -1;
    float virtualW_ = 1600.0f;
    float virtualH_ = 900.0f;
};
