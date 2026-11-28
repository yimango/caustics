#pragma once

#include "render/shaders/common_height.h"

inline constexpr const char* kWaterVertexShader = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;

    out vec3 FragPos;
    out vec3 Normal;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    uniform sampler2D heightTex;
    uniform float heightScale;
    uniform vec2 poolHalfExtent;
    uniform vec2 gridSize;
)" WAVE_HEIGHT_SAMPLE R"(
    void main() {
        vec2 xy = (model * vec4(aPos, 1.0)).xy;
        float h = sampleHeight(xy);
        float span = 8.0;
        float dhdx = (sampleHeight(xy + vec2(span, 0.0)) - sampleHeight(xy - vec2(span, 0.0))) / (span * 2.0);
        float dhdy = (sampleHeight(xy + vec2(0.0, span)) - sampleHeight(xy - vec2(0.0, span))) / (span * 2.0);
        FragPos = vec3(xy, h * heightScale);
        Normal = normalize(vec3(-dhdx * heightScale, -dhdy * heightScale, 1.0));
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)";

inline constexpr const char* kWaterFragmentShader = R"(
    #version 330 core
    in vec3 FragPos;
    in vec3 Normal;

    out vec4 FragColor;

    uniform vec3 lightPos;
    uniform vec3 viewPos;

    void main() {
        vec3 N = normalize(Normal);
        if (N.z < 0.0) {
            N = -N;
        }
        vec3 L = normalize(lightPos - FragPos);
        vec3 V = normalize(viewPos - FragPos);
        vec3 R = reflect(-L, N);

        float ndotv = max(dot(N, V), 0.0);
        float fresnel = pow(1.0 - ndotv, 5.0);
        vec3 color = mix(vec3(0.03, 0.28, 0.36), vec3(0.40, 0.66, 0.78), fresnel);

        float spec = pow(max(dot(V, R), 0.0), 48.0);
        color += vec3(0.85, 0.92, 1.0) * spec * 0.45;

        float alpha = mix(0.28, 0.52, fresnel);
        FragColor = vec4(color, alpha);
    }
)";
