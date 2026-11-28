#pragma once

#include <cstddef>
#include <functional>
#include <vector>
#include "render/shader_program.h"

struct HudSlider {
    const char* label = "";
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float* value = nullptr;
    const char* format = "%.3f";
    std::function<void()> onChange = nullptr;
};

struct HudButton {
    const char* label = "";
    std::function<const char*()> valueText;
    std::function<void()> onClick;
};

struct OverlayVertex {
    float x = 0.0f;
    float y = 0.0f;
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

class OverlayUI {
public:
    ~OverlayUI();

    bool init(float virtualWidth = 1600.0f, float virtualHeight = 900.0f);
    void destroy();

    void setViewportSize(float viewW, float viewH);

    void clearSliders();
    void clearButtons();
    void addSlider(const HudSlider& slider);
    void addButton(const HudButton& button);

    bool onMouseDown(float viewX, float viewY);
    void onMouseDrag(float viewX, float viewY);
    void onMouseUp();
    bool isDragging() const { return activeSlider_ >= 0; }

    void render(float viewW, float viewH) const;
    void render() const { render(viewW_, viewH_); }

    float virtualWidth() const { return viewW_; }
    float virtualHeight() const { return viewH_; }

private:
    int totalRowCount() const;
    float panelTopY(float viewH) const;
    float panelHeight() const;
    float panelBottomY(float viewH) const;
    float rowBottomY(int index, float viewH) const;
    float sliderTrackX() const;
    float sliderValueX() const;
    float sliderTrackW() const;
    int rowAt(float x, float y, float viewH) const;
    void writeSliderAt(int index, float cursorX);

    void appendRect(float x, float y, float w, float h, float r, float g, float b, float a) const;
    void appendGlyph(char code, float x, float y, float scale, float r, float g, float b, float a = 1.0f) const;
    void appendText(const char* text, float x, float y, float scale, float r, float g, float b, float a = 1.0f) const;

    ShaderProgram shader_;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    mutable std::size_t vboCapacityBytes_ = 0;
    mutable std::vector<OverlayVertex> vertices_;
    std::vector<HudSlider> sliders_;
    std::vector<HudButton> buttons_;
    int activeSlider_ = -1;
    mutable float viewW_ = 1600.0f;
    mutable float viewH_ = 900.0f;
};
