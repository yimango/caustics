#pragma once

inline constexpr const char* kOverlayVertexShader = R"(
    #version 330 core
    layout (location = 0) in vec2 aPos;

    uniform vec2 viewport;

    void main() {
        vec2 ndc = vec2(aPos.x / viewport.x * 2.0 - 1.0, aPos.y / viewport.y * 2.0 - 1.0);
        gl_Position = vec4(ndc, 0.0, 1.0);
    }
)";

inline constexpr const char* kOverlayFragmentShader = R"(
    #version 330 core
    uniform vec3 color;
    uniform float alpha;

    out vec4 FragColor;

    void main() {
        FragColor = vec4(color, alpha);
    }
)";
