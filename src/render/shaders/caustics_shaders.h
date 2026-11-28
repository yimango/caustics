#pragma once

#include "render/shaders/common_height.h"

inline constexpr const char* kCausticVertexShader = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;

    smooth out float casts;
    smooth out float intensity;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    uniform vec3 lightPos;
    uniform float bottomZ;
    uniform float waterIOR;
    uniform float airIOR;
    uniform float heightScale;
    uniform vec2 poolHalfExtent;
    uniform sampler2D heightTex;
    uniform vec2 gridSize;
    uniform float meshSpacing;
)" WAVE_HEIGHT_SAMPLE R"(
    bool floorHit(vec2 xy, out vec3 hit) {
        float h = sampleHeight(xy);
        float dhdx;
        float dhdy;
        heightSlope(xy, dhdx, dhdy);
        vec3 optical = normalize(vec3(-dhdx, -dhdy, 1.0));
        vec3 origin = vec3(xy, h);
        vec3 incident = normalize(origin - lightPos);
        float eta = airIOR / max(waterIOR, 0.001);
        vec3 transmitted = refract(incident, optical, eta);
        hit = origin;
        if (transmitted.z >= 0.0) {
            return false;
        }
        float travel = (bottomZ - origin.z) / transmitted.z;
        if (travel <= 0.0) {
            return false;
        }
        hit = origin + transmitted * travel;
        return true;
    }

    void main() {
        vec2 xy = (model * vec4(aPos, 1.0)).xy;
        vec3 hit = vec3(xy, sampleHeight(xy));
        casts = 0.0;
        intensity = 0.0;
        vec3 center;
        if (floorHit(xy, center)) {
            casts = 1.0;
            hit = center;
            float spacing = meshSpacing > 0.0 ? meshSpacing : (poolHalfExtent.x * 2.0) / max(gridSize.x - 1.0, 1.0);
            vec3 hitX;
            vec3 hitY;
            if (floorHit(xy + vec2(spacing, 0.0), hitX) && floorHit(xy + vec2(0.0, spacing), hitY)) {
                vec2 edgeX = hitX.xy - center.xy;
                vec2 edgeY = hitY.xy - center.xy;
                float receiver = abs(edgeX.x * edgeY.y - edgeX.y * edgeY.x);
                float source = spacing * spacing;
                intensity = clamp(source / max(receiver, 0.0001), 0.0, 4.0);
            }
        }
        gl_Position = projection * view * vec4(hit, 1.0);
    }
)";

inline constexpr const char* kCausticFragmentShader = R"(
    #version 330 core
    smooth in float intensity;

    out vec4 FragColor;

    void main() {
        FragColor = vec4(intensity);
    }
)";

inline constexpr const char* kCausticBlurVertexShader = R"(
    #version 330 core
    void main() {
        vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
        gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
    }
)";

inline constexpr const char* kCausticBlurFragmentShader = R"(
    #version 330 core
    out vec4 FragColor;

    uniform sampler2D source;
    uniform vec2 direction;

    void main() {
        float texel = abs(direction.x) + abs(direction.y);
        vec2 uv = gl_FragCoord.xy * texel;
        float sigma = 6.0;
        vec4 sum = vec4(0.0);
        float weightSum = 0.0;
        for (int i = -18; i <= 18; ++i) {
            float x = float(i);
            float w = exp(-0.5 * x * x / (sigma * sigma));
            sum += texture(source, uv + direction * x) * w;
            weightSum += w;
        }
        FragColor = sum / weightSum;
    }
)";
