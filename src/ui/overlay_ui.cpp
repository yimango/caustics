#include "ui/overlay_ui.h"

#include <glad/glad.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "render/shaders/overlay_shaders.h"

namespace {

constexpr float kPanelX = 16.0f;
constexpr float kPanelMarginTop = 14.0f;
constexpr float kPanelW = 380.0f;
constexpr float kPanelPad = 10.0f;
constexpr float kRowH = 28.0f;
constexpr float kGlyphScale = 2.0f;
constexpr float kLabelColumn = 108.0f;
constexpr float kValueColumn = 82.0f;

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
    {'+', {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}},
    {':', {0x00, 0x06, 0x06, 0x00, 0x06, 0x06, 0x00}},
    {'/', {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10}},
    {'%', {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13}},
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {'D', {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}},
    {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'J', {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'Q', {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}},
    {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}},
    {'X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}},
    {'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}},
    {'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}},
};

constexpr int kGlyphCount = static_cast<int>(sizeof(kGlyphs) / sizeof(kGlyphs[0]));

const unsigned char* findGlyph(char code) {
    const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(code)));
    for (int i = 0; i < kGlyphCount; ++i) {
        if (kGlyphs[i].code == upper) {
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
    viewW_ = (virtualWidth > 1.0f) ? virtualWidth : 1600.0f;
    viewH_ = (virtualHeight > 1.0f) ? virtualHeight : 900.0f;

    if (!shader_.build(kOverlayVertexShader, kOverlayFragmentShader, "overlay")) {
        return false;
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    constexpr std::size_t kInitialVerts = 4096;
    vboCapacityBytes_ = kInitialVerts * sizeof(OverlayVertex);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vboCapacityBytes_), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(sizeof(OverlayVertex)),
                          reinterpret_cast<void*>(offsetof(OverlayVertex, x)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(sizeof(OverlayVertex)),
                          reinterpret_cast<void*>(offsetof(OverlayVertex, r)));
    glEnableVertexAttribArray(1);

    vertices_.reserve(kInitialVerts);
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
    vboCapacityBytes_ = 0;
    activeSlider_ = -1;
}

void OverlayUI::setViewportSize(float viewW, float viewH) {
    if (viewW > 1.0f && viewH > 1.0f) {
        viewW_ = viewW;
        viewH_ = viewH;
    }
}

void OverlayUI::clearSliders() {
    sliders_.clear();
    activeSlider_ = -1;
}

void OverlayUI::clearButtons() {
    buttons_.clear();
}

void OverlayUI::addSlider(const HudSlider& slider) {
    sliders_.push_back(slider);
}

void OverlayUI::addButton(const HudButton& button) {
    buttons_.push_back(button);
}

int OverlayUI::totalRowCount() const {
    return static_cast<int>(sliders_.size() + buttons_.size());
}

float OverlayUI::panelTopY(float viewH) const {
    return viewH - kPanelMarginTop;
}

float OverlayUI::panelHeight() const {
    return kPanelPad * 2.0f + kRowH * static_cast<float>(totalRowCount());
}

float OverlayUI::panelBottomY(float viewH) const {
    return panelTopY(viewH) - panelHeight();
}

float OverlayUI::rowBottomY(int index, float viewH) const {
    return panelTopY(viewH) - kPanelPad - kRowH * static_cast<float>(index + 1);
}

float OverlayUI::sliderTrackX() const {
    return kPanelX + kPanelPad + kLabelColumn;
}

float OverlayUI::sliderValueX() const {
    return kPanelX + kPanelW - kPanelPad - kValueColumn;
}

float OverlayUI::sliderTrackW() const {
    return sliderValueX() - 14.0f - sliderTrackX();
}

int OverlayUI::rowAt(float x, float y, float viewH) const {
    const int count = totalRowCount();
    if (count == 0) {
        return -1;
    }
    const float top = panelTopY(viewH);
    const float bottom = panelBottomY(viewH);
    if (x < kPanelX || x >= kPanelX + kPanelW || y < bottom || y >= top) {
        return -1;
    }
    for (int i = 0; i < count; ++i) {
        const float rBottom = rowBottomY(i, viewH);
        if (y >= rBottom && y < rBottom + kRowH) {
            return i;
        }
    }
    if (y >= top - kPanelPad) {
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
    t = std::clamp(t, 0.0f, 1.0f);
    const HudSlider& slider = sliders_[index];
    *slider.value = slider.minValue + t * (slider.maxValue - slider.minValue);
    if (slider.onChange) {
        slider.onChange();
    }
}

bool OverlayUI::onMouseDown(float viewX, float viewY) {
    const int row = rowAt(viewX, viewY, viewH_);
    if (row < 0) {
        return false;
    }
    const int sliderCount = static_cast<int>(sliders_.size());
    if (row < sliderCount) {
        activeSlider_ = row;
        const float trackX = sliderTrackX();
        const float trackW = sliderTrackW();
        if (viewX >= trackX - 6.0f && viewX <= trackX + trackW + 6.0f) {
            writeSliderAt(row, viewX);
        }
        return true;
    }

    const int buttonIndex = row - sliderCount;
    if (buttonIndex >= 0 && buttonIndex < static_cast<int>(buttons_.size())) {
        if (buttons_[buttonIndex].onClick) {
            buttons_[buttonIndex].onClick();
        }
        return true;
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

void OverlayUI::appendRect(float x, float y, float w, float h, float r, float g, float b, float a) const {
    if (w <= 0.0f || h <= 0.0f) {
        return;
    }
    vertices_.push_back({x, y, r, g, b, a});
    vertices_.push_back({x + w, y, r, g, b, a});
    vertices_.push_back({x + w, y + h, r, g, b, a});
    vertices_.push_back({x, y, r, g, b, a});
    vertices_.push_back({x + w, y + h, r, g, b, a});
    vertices_.push_back({x, y + h, r, g, b, a});
}

void OverlayUI::appendGlyph(char code, float x, float y, float scale, float r, float g, float b, float a) const {
    const unsigned char* rows = findGlyph(code);
    for (int row = 0; row < 7; ++row) {
        const unsigned char bits = rows[row];
        if (bits == 0) {
            continue;
        }
        for (int col = 0; col < 5; ++col) {
            if ((bits & static_cast<unsigned char>(0x10 >> col)) == 0) {
                continue;
            }
            appendRect(x + static_cast<float>(col) * scale,
                       y + static_cast<float>(6 - row) * scale,
                       scale, scale, r, g, b, a);
        }
    }
}

void OverlayUI::appendText(const char* text, float x, float y, float scale, float r, float g, float b, float a) const {
    if (text == nullptr) {
        return;
    }
    float pen = x;
    for (int i = 0; text[i] != '\0'; ++i) {
        appendGlyph(text[i], pen, y, scale, r, g, b, a);
        pen += 6.0f * scale;
    }
}

void OverlayUI::render(float viewW, float viewH) const {
    if (totalRowCount() == 0 || !shader_.valid() || viewW <= 1.0f || viewH <= 1.0f) {
        return;
    }
    viewW_ = viewW;
    viewH_ = viewH;

    vertices_.clear();

    const float pBottom = panelBottomY(viewH_);
    const float pHeight = panelHeight();

    // Panel background and subtle border
    appendRect(kPanelX - 1.0f, pBottom - 1.0f, kPanelW + 2.0f, pHeight + 2.0f, 0.24f, 0.34f, 0.38f, 0.55f);
    appendRect(kPanelX, pBottom, kPanelW, pHeight, 0.05f, 0.08f, 0.10f, 0.82f);
    appendRect(kPanelX, pBottom + pHeight - 2.0f, kPanelW, 2.0f, 0.38f, 0.68f, 0.64f, 0.85f);

    const int sliderCount = static_cast<int>(sliders_.size());
    for (int i = 0; i < sliderCount; ++i) {
        const HudSlider& slider = sliders_[i];
        const float bottom = rowBottomY(i, viewH_);
        const float glyphY = bottom + (kRowH - 7.0f * kGlyphScale) * 0.5f;
        appendText(slider.label, kPanelX + kPanelPad, glyphY, kGlyphScale, 0.86f, 0.91f, 0.90f);

        const float trackX = sliderTrackX();
        const float trackW = sliderTrackW();
        const float trackH = 5.0f;
        const float trackY = bottom + (kRowH - trackH) * 0.5f;
        appendRect(trackX, trackY, trackW, trackH, 0.17f, 0.22f, 0.25f, 1.0f);

        float val = (slider.value != nullptr) ? *slider.value : slider.minValue;
        float t = 0.0f;
        if (slider.maxValue > slider.minValue) {
            t = std::clamp((val - slider.minValue) / (slider.maxValue - slider.minValue), 0.0f, 1.0f);
        }
        appendRect(trackX, trackY, trackW * t, trackH, 0.42f, 0.72f, 0.66f, 1.0f);

        const float thumbW = 7.0f;
        const float thumbH = 16.0f;
        const float thumbX = trackX + trackW * t - thumbW * 0.5f;
        const float thumbY = bottom + (kRowH - thumbH) * 0.5f;
        appendRect(thumbX, thumbY, thumbW, thumbH, 0.94f, 0.96f, 0.94f, 1.0f);

        char valueText[24];
        const char* fmt = (slider.format != nullptr) ? slider.format : "%.3f";
        std::snprintf(valueText, sizeof(valueText), fmt, val);
        appendText(valueText, sliderValueX(), glyphY, kGlyphScale, 0.86f, 0.91f, 0.90f);
    }

    const int buttonCount = static_cast<int>(buttons_.size());
    for (int i = 0; i < buttonCount; ++i) {
        const HudButton& button = buttons_[i];
        const int rowIndex = sliderCount + i;
        const float bottom = rowBottomY(rowIndex, viewH_);
        const float glyphY = bottom + (kRowH - 7.0f * kGlyphScale) * 0.5f;
        appendText(button.label, kPanelX + kPanelPad, glyphY, kGlyphScale, 0.86f, 0.91f, 0.90f);

        const float btnX = sliderTrackX();
        const float btnW = (kPanelX + kPanelW - kPanelPad) - btnX;
        const float btnH = kRowH - 6.0f;
        const float btnY = bottom + 3.0f;

        appendRect(btnX - 1.0f, btnY - 1.0f, btnW + 2.0f, btnH + 2.0f, 0.42f, 0.72f, 0.66f, 0.65f);
        appendRect(btnX, btnY, btnW, btnH, 0.14f, 0.24f, 0.27f, 0.95f);

        const char* valStr = button.valueText ? button.valueText() : "";
        const std::size_t len = (valStr != nullptr) ? std::strlen(valStr) : 0;
        const float textW = (len > 0) ? (static_cast<float>(len) * 6.0f - 1.0f) * kGlyphScale : 0.0f;
        const float textX = btnX + std::max(6.0f, (btnW - textW) * 0.5f);
        appendText(valStr, textX, glyphY, kGlyphScale, 0.94f, 0.98f, 0.96f);
    }

    if (vertices_.empty()) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_.use();
    shader_.setVec2("viewport", glm::vec2(viewW_, viewH_));

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    const std::size_t byteSize = vertices_.size() * sizeof(OverlayVertex);
    if (byteSize > vboCapacityBytes_) {
        vboCapacityBytes_ = std::max(byteSize, vboCapacityBytes_ * 2);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vboCapacityBytes_), nullptr, GL_DYNAMIC_DRAW);
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(byteSize), vertices_.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size()));

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
