#include "ui/overlay_ui.h"

#include <glad/glad.h>
#include <cstdio>

#include "render/shaders/overlay_shaders.h"

namespace {

constexpr float kPanelX = 16.0f;
constexpr float kPanelW = 300.0f;
constexpr float kPanelPad = 8.0f;
constexpr float kRowH = 32.0f;
constexpr float kGlyphScale = 3.0f;
constexpr float kLabelColumn = 96.0f;
constexpr float kValueColumn = 96.0f;

struct Glyph {
    char code;
    unsigned char rows[7];
};

const Glyph kGlyphs[] = {
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

constexpr int kGlyphCount = static_cast<int>(sizeof(kGlyphs) / sizeof(kGlyphs[0]));

const unsigned char* findGlyph(char code) {
    for (int i = 0; i < kGlyphCount; ++i) {
        if (kGlyphs[i].code == code) {
            return kGlyphs[i].rows;
        }
    }
    return kGlyphs[0].rows;
}

} // namespace

OverlayUI::~OverlayUI() {
    destroy();
}

bool OverlayUI::init(float virtualWidth, float virtualHeight) {
    destroy();
    virtualW_ = virtualWidth;
    virtualH_ = virtualHeight;

    if (!shader_.build(kOverlayVertexShader, kOverlayFragmentShader, "overlay")) {
        return false;
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, 12 * static_cast<GLsizeiptr>(sizeof(float)), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    return true;
}

void OverlayUI::destroy() {
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    shader_.destroy();
    activeSlider_ = -1;
}

void OverlayUI::clearSliders() {
    sliders_.clear();
    activeSlider_ = -1;
}

void OverlayUI::addSlider(const HudSlider& slider) {
    sliders_.push_back(slider);
}

float OverlayUI::panelTopY() const {
    return virtualH_ - 12.0f;
}

float OverlayUI::panelHeight() const {
    return kPanelPad * 2.0f + kRowH * static_cast<float>(sliders_.size());
}

float OverlayUI::panelBottomY() const {
    return panelTopY() - panelHeight();
}

float OverlayUI::rowBottomY(int index) const {
    return panelTopY() - kPanelPad - kRowH * static_cast<float>(index + 1);
}

float OverlayUI::sliderTrackX() const {
    return kPanelX + kPanelPad + kLabelColumn;
}

float OverlayUI::sliderValueX() const {
    return kPanelX + kPanelW - kPanelPad - kValueColumn;
}

float OverlayUI::sliderTrackW() const {
    return sliderValueX() - 8.0f - sliderTrackX();
}

int OverlayUI::sliderRowAt(float x, float y) const {
    if (sliders_.empty()) {
        return -1;
    }
    if (x < kPanelX || x >= kPanelX + kPanelW || y < panelBottomY() || y >= panelTopY()) {
        return -1;
    }
    const int count = static_cast<int>(sliders_.size());
    for (int i = 0; i < count; ++i) {
        const float bottom = rowBottomY(i);
        if (y >= bottom && y < bottom + kRowH) {
            return i;
        }
    }
    if (y >= panelTopY() - kPanelPad) {
        return 0;
    }
    return count - 1;
}

void OverlayUI::writeSliderAt(int index, float cursorX) {
    if (index < 0 || index >= static_cast<int>(sliders_.size()) || sliders_[index].value == nullptr) {
        return;
    }
    const float span = sliderTrackW();
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
    const HudSlider& slider = sliders_[index];
    *slider.value = slider.minValue + t * (slider.maxValue - slider.minValue);
}

bool OverlayUI::onMouseDown(float viewX, float viewY) {
    const int row = sliderRowAt(viewX, viewY);
    if (row < 0) {
        return false;
    }
    activeSlider_ = row;
    const float trackX = sliderTrackX();
    const float trackW = sliderTrackW();
    if (viewX >= trackX && viewX <= trackX + trackW) {
        writeSliderAt(row, viewX);
    }
    return true;
}

void OverlayUI::onMouseDrag(float viewX, float /*viewY*/) {
    if (activeSlider_ >= 0) {
        writeSliderAt(activeSlider_, viewX);
    }
}

void OverlayUI::onMouseUp() {
    activeSlider_ = -1;
}

void OverlayUI::drawRect(float x, float y, float w, float h, float r, float g, float b, float a) const {
    const float verts[12] = {
        x, y,
        x + w, y,
        x + w, y + h,
        x, y,
        x + w, y + h,
        x, y + h
    };
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    shader_.setVec3("color", glm::vec3(r, g, b));
    shader_.setFloat("alpha", a);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void OverlayUI::drawGlyph(char code, float x, float y, float scale, float r, float g, float b) const {
    const unsigned char* rows = findGlyph(code);
    for (int row = 0; row < 7; ++row) {
        const unsigned char bits = rows[row];
        for (int col = 0; col < 5; ++col) {
            if ((bits & static_cast<unsigned char>(0x10 >> col)) == 0) {
                continue;
            }
            drawRect(x + static_cast<float>(col) * scale,
                     y + static_cast<float>(6 - row) * scale,
                     scale, scale, r, g, b, 1.0f);
        }
    }
}

void OverlayUI::drawText(const char* text, float x, float y, float scale, float r, float g, float b) const {
    float pen = x;
    for (int i = 0; text[i] != '\0'; ++i) {
        drawGlyph(text[i], pen, y, scale, r, g, b);
        pen += 6.0f * scale;
    }
}

void OverlayUI::render() const {
    if (sliders_.empty() || !shader_.valid()) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_.use();
    shader_.setVec2("viewport", glm::vec2(virtualW_, virtualH_));
    glBindVertexArray(vao_);

    drawRect(kPanelX, panelBottomY(), kPanelW, panelHeight(), 0.06f, 0.08f, 0.09f, 0.82f);

    const int count = static_cast<int>(sliders_.size());
    for (int i = 0; i < count; ++i) {
        const HudSlider& slider = sliders_[i];
        const float bottom = rowBottomY(i);
        const float glyphY = bottom + (kRowH - 7.0f * kGlyphScale) * 0.5f;
        drawText(slider.label, kPanelX + kPanelPad, glyphY, kGlyphScale, 0.86f, 0.90f, 0.88f);

        const float trackX = sliderTrackX();
        const float trackW = sliderTrackW();
        const float trackH = 6.0f;
        const float trackY = bottom + (kRowH - trackH) * 0.5f;
        drawRect(trackX, trackY, trackW, trackH, 0.18f, 0.22f, 0.24f, 1.0f);

        float val = (slider.value != nullptr) ? *slider.value : slider.minValue;
        float t = 0.0f;
        if (slider.maxValue > slider.minValue) {
            t = (val - slider.minValue) / (slider.maxValue - slider.minValue);
        }
        if (t < 0.0f) {
            t = 0.0f;
        }
        if (t > 1.0f) {
            t = 1.0f;
        }
        drawRect(trackX, trackY, trackW * t, trackH, 0.45f, 0.66f, 0.60f, 1.0f);

        const float thumbW = 8.0f;
        const float thumbH = 18.0f;
        const float thumbX = trackX + trackW * t - thumbW * 0.5f;
        const float thumbY = bottom + (kRowH - thumbH) * 0.5f;
        drawRect(thumbX, thumbY, thumbW, thumbH, 0.93f, 0.95f, 0.92f, 1.0f);

        char valueText[16];
        const char* fmt = (slider.format != nullptr) ? slider.format : "%.3f";
        std::snprintf(valueText, sizeof(valueText), fmt, val);
        drawText(valueText, sliderValueX(), glyphY, kGlyphScale, 0.86f, 0.90f, 0.88f);
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
