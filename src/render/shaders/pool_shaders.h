#pragma once

#include "render/pool_optics.h"
#include "render/shaders/common_height.h"

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
        Normal = normalize(mat3(model) * aNormal);
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)";

inline constexpr const char* kPoolFragmentShader = R"(
    #version 330 core
    in vec3 FragPos;
    in vec3 Normal;

    out vec4 FragColor;

    uniform sampler2D causticsTexture;
    uniform sampler2D heightTex;
    uniform vec2 poolHalfExtent;
    uniform vec2 gridSize;
    uniform float bottomZ;
    uniform float lipZ;
    uniform float heightScale;
    uniform vec3 lightPos;
    uniform vec3 lightColor;
    uniform vec3 viewPos;
    uniform float causticStrength;
    uniform float turbidity;
    uniform float exposure;
)" WAVE_HEIGHT_SAMPLE POOL_OPTICS_GLSL R"(
    float hash12(vec2 p) {
        vec3 p3 = fract(vec3(p.xyx) * 0.1031);
        p3 += dot(p3, p3.yzx + 33.33);
        return fract((p3.x + p3.y) * p3.z);
    }

    vec3 softHighlightShoulder(vec3 c) {
        const float s = 0.80;
        const float hi = 0.98;
        const float span = hi - s;
        vec3 over = max(c - vec3(s), vec3(0.0));
        vec3 rolled = vec3(s) + vec3(span) * (vec3(1.0) - exp(-over / span));
        return mix(c, rolled, step(vec3(s), c));
    }

    void main() {
        vec3 N = normalize(Normal);
        vec3 L = normalize(lightPos - FragPos);
        vec3 V = normalize(viewPos - FragPos);
        float ndotl = max(dot(N, L), 0.0);
        float rimVis = rimVisibility(FragPos, lightPos, poolHalfExtent, lipZ, 1.5);
        vec3 kAbs = vec3(0.005, 0.0016, 0.0010) * (1.0 + 1.5 * turbidity);

        if (N.z > 0.5) {
            vec2 causticsUV = poolPointToAtlasUV(FragPos, POOL_FACE_FLOOR, poolHalfExtent, bottomZ, lipZ);
            vec3 causticRGB = texture(causticsTexture, causticsUV).rgb;

            vec2 tile = FragPos.xy / 25.0;
            vec2 cell = fract(tile);
            vec2 fw = max(fwidth(tile), vec2(0.002));
            float gx = smoothstep(0.025 - fw.x, 0.025 + fw.x, cell.x) *
                       smoothstep(0.025 - fw.x, 0.025 + fw.x, 1.0 - cell.x);
            float gy = smoothstep(0.025 - fw.y, 0.025 + fw.y, cell.y) *
                       smoothstep(0.025 - fw.y, 0.025 + fw.y, 1.0 - cell.y);
            float tileMask = gx * gy;

            vec3 albedo = vec3(0.62, 0.70, 0.66);
            if (mod(floor(tile.x) + floor(tile.y), 2.0) > 0.5) {
                albedo = vec3(0.33, 0.50, 0.52);
            }
            albedo = mix(vec3(0.16, 0.22, 0.24), albedo, tileMask);

            float distEdge = min(poolHalfExtent.x - abs(FragPos.x), poolHalfExtent.y - abs(FragPos.y));
            float cornerAO = mix(0.68, 1.0, smoothstep(0.0, 14.0, distEdge));

            float waterZ = sampleHeight(FragPos.xy) * heightScale;
            float depth = max(waterZ - FragPos.z, 0.0);
            vec3 depthTint = exp(-depth * kAbs);

            vec3 cTone = causticRGB / (vec3(1.0) + 0.75 * max(causticRGB - vec3(0.60), vec3(0.0)));
            vec3 ambient = vec3(0.30, 0.34, 0.36) * cornerAO;
            vec3 direct = lightColor * (0.14 * ndotl * rimVis);
            vec3 causticContrib = lightColor * (causticStrength * cTone);

            vec3 color = albedo * (ambient + direct + causticContrib) * depthTint * exposure;
            color = softHighlightShoulder(color);
            FragColor = vec4(clamp(color, 0.0, 0.985), 1.0);
            return;
        }

        // Wall fragment: identify face from inward normal.
        int face = POOL_FACE_SOUTH;
        float wallU = FragPos.x;
        float wallHalfU = poolHalfExtent.x;
        if (N.y > 0.5) {
            face = POOL_FACE_SOUTH;
            wallU = FragPos.x;
            wallHalfU = poolHalfExtent.x;
        } else if (N.x < -0.5) {
            face = POOL_FACE_EAST;
            wallU = FragPos.y;
            wallHalfU = poolHalfExtent.y;
        } else if (N.y < -0.5) {
            face = POOL_FACE_NORTH;
            wallU = FragPos.x;
            wallHalfU = poolHalfExtent.x;
        } else {
            face = POOL_FACE_WEST;
            wallU = FragPos.y;
            wallHalfU = poolHalfExtent.y;
        }

        vec2 clampedXY = clamp(FragPos.xy, -poolHalfExtent, poolHalfExtent);
        float waterZ = sampleHeight(clampedXY) * heightScale;

        // Masonry / pool tile pattern along the wall.
        float zRel = FragPos.z - bottomZ;
        float courseH = 12.0;
        float blockW = 25.0;
        float courseId = floor(zRel / courseH);
        float stagger = mod(courseId, 2.0) * 0.5;
        vec2 wallTile = vec2(wallU / blockW + stagger, zRel / courseH);
        vec2 wallCell = fract(wallTile);
        vec2 wfw = max(fwidth(wallTile), vec2(0.002));
        float mortarX = smoothstep(0.035 - wfw.x, 0.035 + wfw.x, wallCell.x) *
                        smoothstep(0.035 - wfw.x, 0.035 + wfw.x, 1.0 - wallCell.x);
        float mortarY = smoothstep(0.055 - wfw.y, 0.055 + wfw.y, wallCell.y) *
                        smoothstep(0.055 - wfw.y, 0.055 + wfw.y, 1.0 - wallCell.y);
        float blockMask = mortarX * mortarY;

        float blockJitter = hash12(floor(wallTile) + vec2(float(face) * 17.0, 3.0));
        vec3 dryStone = mix(vec3(0.56, 0.59, 0.57), vec3(0.64, 0.66, 0.63), blockJitter);
        if (FragPos.z > lipZ - 3.0) {
            dryStone = mix(dryStone, vec3(0.70, 0.69, 0.64), smoothstep(lipZ - 3.0, lipZ - 2.0, FragPos.z));
        }
        dryStone = mix(vec3(0.32, 0.35, 0.36), dryStone, blockMask);

        float distFloor = max(FragPos.z - bottomZ, 0.0);
        float distCorner = max(wallHalfU - abs(wallU), 0.0);
        float floorAO = mix(0.70, 1.0, smoothstep(0.0, 10.0, distFloor));
        float cornerAO = mix(0.78, 1.0, smoothstep(0.0, 12.0, distCorner));
        float ao = floorAO * cornerAO;

        vec3 color;
        if (FragPos.z < waterZ) {
            vec2 wallCausticUV = poolPointToAtlasUV(FragPos, face, poolHalfExtent, bottomZ, lipZ);
            vec3 causticRGB = texture(causticsTexture, wallCausticUV).rgb;
            float waterlineFade = smoothstep(waterZ + 0.10, waterZ - 0.60, FragPos.z);
            causticRGB *= waterlineFade;

            vec3 wetAlbedo = dryStone * vec3(0.66, 0.74, 0.78);
            float subDepth = max(waterZ - FragPos.z, 0.0);
            vec3 depthTint = exp(-subDepth * kAbs);

            vec3 cTone = causticRGB / (vec3(1.0) + 0.75 * max(causticRGB - vec3(0.55), vec3(0.0)));
            vec3 ambient = vec3(0.28, 0.34, 0.38) * ao;
            vec3 direct = lightColor * (0.20 * ndotl * rimVis);
            vec3 causticContrib = lightColor * (causticStrength * 1.15 * cTone);

            color = wetAlbedo * (ambient + direct + causticContrib) * depthTint;
        } else {
            // Dry wall with subtle wet-wicking transition immediately above waterline.
            float aboveWater = FragPos.z - waterZ;
            float dryBlend = smoothstep(0.0, 1.8, aboveWater);
            vec3 wallAlbedo = mix(dryStone * vec3(0.72, 0.77, 0.80), dryStone, dryBlend);

            vec3 skyAmb = vec3(0.28, 0.33, 0.38) * ao;
            vec3 direct = lightColor * (0.62 * ndotl * rimVis);
            color = wallAlbedo * (skyAmb + direct);
        }

        // Dynamic waterline meniscus contact band tracing the live wave height.
        float distWaterline = FragPos.z - waterZ;
        if (abs(distWaterline) < 1.2) {
            float meniscusBand = exp(-pow(distWaterline / 0.45, 2.0));
            float darkLip = exp(-pow((distWaterline + 0.55) / 0.35, 2.0));
            vec3 H = normalize(L + V);
            float meniscusSpec = pow(max(dot(N, H), 0.0), 24.0) * rimVis;
            color *= (1.0 - 0.14 * darkLip);
            color += lightColor * (0.12 + 0.24 * meniscusSpec) * meniscusBand;
        }

        color = softHighlightShoulder(color * exposure);
        FragColor = vec4(clamp(color, 0.0, 0.985), 1.0);
    }
)";
