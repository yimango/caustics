#pragma once

#include "render/pool_optics.h"
#include "render/shaders/common_height.h"

inline constexpr const char* kCausticVertexShader = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;

    smooth out vec3 vOldPos;
    smooth out vec2 vUnfoldedPos;
    smooth out vec4 vWallMask;
    smooth out float vValid;
    smooth out float vFluxWeight;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    uniform vec3 lightPos;
    uniform float bottomZ;
    uniform float lipZ;
    uniform float waterIOR;
    uniform float airIOR;
    uniform float heightScale;
    uniform vec2 poolHalfExtent;
    uniform sampler2D heightTex;
    uniform vec2 gridSize;
)" WAVE_HEIGHT_SAMPLE POOL_OPTICS_GLSL R"(
    void main() {
        vec2 xy = (model * vec4(aPos, 1.0)).xy;
        float hRaw;
        vec2 gradRaw;
        sampleHeightAndGrad(xy, hRaw, gradRaw);

        float hWorld = clamp(hRaw * heightScale, bottomZ + 1.0, lipZ - 0.5);
        vec2 gradWorld = gradRaw * (heightScale * 0.55);
        vec3 N = normalize(vec3(-gradWorld.x, -gradWorld.y, 1.0));
        vec3 origin = vec3(xy, hWorld);

        vec3 incident = normalize(origin - lightPos);
        float cosI = dot(-incident, N);
        float eta = airIOR / max(waterIOR, 0.001);

        vOldPos = origin;
        vUnfoldedPos = xy;
        vWallMask = vec4(0.0);
        vValid = 0.0;
        vFluxWeight = 0.0;

        if (cosI > 0.001) {
            vec3 transmitted = refract(incident, N, eta);
            if (dot(transmitted, transmitted) > 1e-4) {
                vec3 hitPos;
                int hitFace;
                if (poolBoxHit(origin, transmitted, poolHalfExtent, bottomZ, lipZ, hitPos, hitFace)) {
                    float r0 = (airIOR - waterIOR) / max(airIOR + waterIOR, 0.001);
                    float f0 = r0 * r0;
                    float fresnel = f0 + (1.0 - f0) * pow(clamp(1.0 - cosI, 0.0, 1.0), 5.0);
                    float transmit = clamp(1.0 - fresnel, 0.0, 1.0);
                    float rimVis = rimVisibility(origin, lightPos, poolHalfExtent, lipZ, 1.5);

                    vUnfoldedPos = unfoldPoolPoint(hitPos, hitFace, poolHalfExtent, bottomZ);
                    vWallMask = poolWallMask(hitFace);
                    vValid = 1.0;
                    vFluxWeight = transmit * cosI * rimVis;
                }
            }
        }

        gl_Position = projection * view * vec4(vUnfoldedPos, 0.0, 1.0);
    }
)";

inline constexpr const char* kCausticFragmentShader = R"(
    #version 330 core
    smooth in vec3 vOldPos;
    smooth in vec2 vUnfoldedPos;
    smooth in vec4 vWallMask;
    smooth in float vValid;
    smooth in float vFluxWeight;

    out vec4 FragColor;

    uniform vec2 poolHalfExtent;
    uniform vec3 channelMask;

    void main() {
        if (vValid < 0.999) {
            discard;
        }
        // Discard triangles jumping across a wall-to-wall corner seam.
        float activeWalls =
            step(1e-4, vWallMask.x) +
            step(1e-4, vWallMask.y) +
            step(1e-4, vWallMask.z) +
            step(1e-4, vWallMask.w);
        if (activeWalls > 1.5) {
            discard;
        }
        if (abs(vUnfoldedPos.x) > poolHalfExtent.x && abs(vUnfoldedPos.y) > poolHalfExtent.y) {
            discard;
        }

        vec3 dOldX = dFdx(vOldPos);
        vec3 dOldY = dFdy(vOldPos);
        float oldArea = length(cross(dOldX, dOldY));

        vec2 dNewX = dFdx(vUnfoldedPos);
        vec2 dNewY = dFdy(vUnfoldedPos);
        float newArea = abs(dNewX.x * dNewY.y - dNewX.y * dNewY.x);

        float ratio = clamp(oldArea / max(newArea, 1e-4), 0.0, 5.0);
        float flux = ratio * max(vFluxWeight, 0.0);
        FragColor = vec4(channelMask * flux, 1.0);
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
        float sigma = 1.8;
        vec4 sum = vec4(0.0);
        float weightSum = 0.0;
        for (int i = -5; i <= 5; ++i) {
            float x = float(i);
            float w = exp(-0.5 * x * x / (sigma * sigma));
            sum += texture(source, uv + direction * x) * w;
            weightSum += w;
        }
        FragColor = sum / weightSum;
    }
)";
