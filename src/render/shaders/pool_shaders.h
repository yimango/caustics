#pragma once

inline constexpr const char* kPoolVertexShader = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aNormal;

    out vec3 FragPos;
    out vec3 Normal;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main() {
        FragPos = vec3(model * vec4(aPos, 1.0));
        Normal = aNormal;
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)";

inline constexpr const char* kPoolFragmentShader = R"(
    #version 330 core
    in vec3 FragPos;
    in vec3 Normal;

    out vec4 FragColor;

    uniform sampler2D causticsTexture;
    uniform vec2 poolHalfExtent;
    uniform float bottomZ;

    void main() {
        if (FragPos.z > bottomZ + 0.5) {
            float band = fract(FragPos.z * 0.15);
            vec3 stone = mix(vec3(0.45, 0.48, 0.46), vec3(0.28, 0.34, 0.36), band);
            FragColor = vec4(stone, 1.0);
            return;
        }
        vec2 causticsUV = (FragPos.xy + poolHalfExtent) / (poolHalfExtent * 2.0);
        float light = texture(causticsTexture, causticsUV).r;
        vec2 tile = FragPos.xy / 25.0;
        vec2 cell = fract(tile);
        float groutLine = 0.06;
        vec3 albedo = vec3(0.62, 0.70, 0.66);
        if (mod(floor(tile.x) + floor(tile.y), 2.0) > 0.5) {
            albedo = vec3(0.33, 0.50, 0.52);
        }
        if (cell.x < groutLine || cell.y < groutLine) {
            albedo = vec3(0.16, 0.22, 0.24);
        }
        float tone = light / (1.0 + 0.85 * max(light - 0.65, 0.0));
        vec3 color = albedo * (0.42 + 1.15 * tone);
        FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
    }
)";
