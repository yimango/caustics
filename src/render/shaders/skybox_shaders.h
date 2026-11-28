#pragma once

inline constexpr const char* kSkyboxVertexShader = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;

    out vec3 TexCoords;

    uniform mat4 projection;
    uniform mat4 view;

    void main() {
        TexCoords = aPos;
        vec4 pos = projection * view * vec4(aPos, 1.0);
        gl_Position = pos.xyww;
    }
)";

inline constexpr const char* kSkyboxFragmentShader = R"(
    #version 330 core
    in vec3 TexCoords;

    out vec4 FragColor;

    void main() {
        float skyFactor = normalize(TexCoords).z * 0.5 + 0.5;
        vec3 skyColor = mix(vec3(0.20, 0.34, 0.46), vec3(0.38, 0.54, 0.68), skyFactor);
        FragColor = vec4(skyColor, 1.0);
    }
)";
