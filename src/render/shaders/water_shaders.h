#pragma once

#define WATER_ANALYTICAL_HEIGHT R"GLSL(
    float waterCatmullRom(float p0, float p1, float p2, float p3, float t) {
        float t2 = t * t;
        float t3 = t2 * t;
        return 0.5 * ((2.0 * p1) +
            (-p0 + p2) * t +
            (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
            (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3);
    }

    float waterCatmullRomDeriv(float p0, float p1, float p2, float p3, float t) {
        float t2 = t * t;
        return 0.5 * ((-p0 + p2) +
            2.0 * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t +
            3.0 * (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t2);
    }

    int waterReflectIndex(int idx, int maxIdx) {
        if (idx < 0) {
            idx = -idx;
        }
        if (idx > maxIdx) {
            idx = 2 * maxIdx - idx;
        }
        return clamp(idx, 0, maxIdx);
    }

    float waterTapHeight(int ix, int iy, ivec2 maxIdx) {
        ivec2 coord = ivec2(waterReflectIndex(ix, maxIdx.x), waterReflectIndex(iy, maxIdx.y));
        return texelFetch(heightTex, coord, 0).r;
    }

    void sampleWaterHeightAndGrad(vec2 xy, out float h, out vec2 grad) {
        vec2 span = max(poolHalfExtent * 2.0, vec2(1e-4));
        vec2 maxGrid = max(gridSize - vec2(1.0), vec2(1.0));
        vec2 norm = clamp((xy + poolHalfExtent) / span, vec2(0.0), vec2(1.0));
        vec2 index = norm * maxGrid;

        vec2 maxCell = max(gridSize - vec2(2.0), vec2(0.0));
        vec2 cell = min(floor(index), maxCell);
        vec2 t = clamp(index - cell, vec2(0.0), vec2(1.0));

        ivec2 maxIdx = max(textureSize(heightTex, 0) - ivec2(1), ivec2(0));
        int cx = int(cell.x);
        int cy = int(cell.y);

        int x0 = cx - 1;
        int x1 = cx;
        int x2 = cx + 1;
        int x3 = cx + 2;

        float r0, r1, r2, r3;
        float dr0, dr1, dr2, dr3;

        {
            int y = cy - 1;
            float p0 = waterTapHeight(x0, y, maxIdx);
            float p1 = waterTapHeight(x1, y, maxIdx);
            float p2 = waterTapHeight(x2, y, maxIdx);
            float p3 = waterTapHeight(x3, y, maxIdx);
            r0 = waterCatmullRom(p0, p1, p2, p3, t.x);
            dr0 = waterCatmullRomDeriv(p0, p1, p2, p3, t.x);
        }
        {
            int y = cy;
            float p0 = waterTapHeight(x0, y, maxIdx);
            float p1 = waterTapHeight(x1, y, maxIdx);
            float p2 = waterTapHeight(x2, y, maxIdx);
            float p3 = waterTapHeight(x3, y, maxIdx);
            r1 = waterCatmullRom(p0, p1, p2, p3, t.x);
            dr1 = waterCatmullRomDeriv(p0, p1, p2, p3, t.x);
        }
        {
            int y = cy + 1;
            float p0 = waterTapHeight(x0, y, maxIdx);
            float p1 = waterTapHeight(x1, y, maxIdx);
            float p2 = waterTapHeight(x2, y, maxIdx);
            float p3 = waterTapHeight(x3, y, maxIdx);
            r2 = waterCatmullRom(p0, p1, p2, p3, t.x);
            dr2 = waterCatmullRomDeriv(p0, p1, p2, p3, t.x);
        }
        {
            int y = cy + 2;
            float p0 = waterTapHeight(x0, y, maxIdx);
            float p1 = waterTapHeight(x1, y, maxIdx);
            float p2 = waterTapHeight(x2, y, maxIdx);
            float p3 = waterTapHeight(x3, y, maxIdx);
            r3 = waterCatmullRom(p0, p1, p2, p3, t.x);
            dr3 = waterCatmullRomDeriv(p0, p1, p2, p3, t.x);
        }

        h = waterCatmullRom(r0, r1, r2, r3, t.y);
        float dhdu = waterCatmullRom(dr0, dr1, dr2, dr3, t.y);
        float dhdv = waterCatmullRomDeriv(r0, r1, r2, r3, t.y);

        vec2 indexPerWorld = maxGrid / span;
        grad = vec2(dhdu * indexPerWorld.x, dhdv * indexPerWorld.y);
    }
)GLSL"

inline constexpr const char* kWaterVertexShader = R"GLSL(
    #version 330 core
    layout (location = 0) in vec3 aPos;

    out vec3 FragPos;
    out vec2 WaterXY;
    out vec3 VertNormal;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    uniform vec3 viewPos;
    uniform sampler2D heightTex;
    uniform float heightScale;
    uniform vec2 poolHalfExtent;
    uniform vec2 gridSize;
    uniform float borderOverlap;
)GLSL" WATER_ANALYTICAL_HEIGHT R"GLSL(
    void main() {
        vec2 xy = (model * vec4(aPos, 1.0)).xy;
        vec2 clampedXY = clamp(xy, -poolHalfExtent, poolHalfExtent);

        float h = 0.0;
        vec2 grad = vec2(0.0);
        sampleWaterHeightAndGrad(clampedXY, h, grad);

        vec2 renderXY = xy;
        float edgeEps = 1e-3;
        if (xy.x <= -poolHalfExtent.x + edgeEps && viewPos.x > -poolHalfExtent.x) {
            renderXY.x -= borderOverlap;
        } else if (xy.x >= poolHalfExtent.x - edgeEps && viewPos.x < poolHalfExtent.x) {
            renderXY.x += borderOverlap;
        }
        if (xy.y <= -poolHalfExtent.y + edgeEps && viewPos.y > -poolHalfExtent.y) {
            renderXY.y -= borderOverlap;
        } else if (xy.y >= poolHalfExtent.y - edgeEps && viewPos.y < poolHalfExtent.y) {
            renderXY.y += borderOverlap;
        }

        FragPos = vec3(renderXY, h * heightScale);
        WaterXY = clampedXY;
        VertNormal = normalize(vec3(-grad.x * heightScale, -grad.y * heightScale, 1.0));
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)GLSL";

inline constexpr const char* kWaterFragmentShader = R"GLSL(
    #version 330 core
    in vec3 FragPos;
    in vec2 WaterXY;
    in vec3 VertNormal;

    out vec4 FragColor;

    uniform sampler2D heightTex;
    uniform sampler2D sceneColorTex;
    uniform sampler2D sceneDepthTex;
    uniform int hasSceneTargets;
    uniform vec2 sceneResolution;
    uniform mat4 viewProj;
    uniform mat4 invViewProj;
    uniform vec2 nearFar;

    uniform vec3 lightPos;
    uniform vec3 lightColor;
    uniform float lightIntensity;
    uniform vec3 viewPos;

    uniform float heightScale;
    uniform vec2 poolHalfExtent;
    uniform vec2 gridSize;
    uniform float bottomZ;
    uniform float lipZ;

    uniform float waterIOR;
    uniform float airIOR;
    uniform float dispersion;
    uniform float turbidity;
    uniform float exposure;
)GLSL" WATER_ANALYTICAL_HEIGHT R"GLSL(
    float linearizeDepth(float depth01) {
        float zNdc = depth01 * 2.0 - 1.0;
        float denom = nearFar.y + nearFar.x - zNdc * (nearFar.y - nearFar.x);
        return (2.0 * nearFar.x * nearFar.y) / max(denom, 1e-4);
    }

    vec2 computeMeniscusTilt(vec2 xy) {
        const float meniscusWidth = 2.5;
        const float meniscusStrength = 0.26;
        float dWest  = max(poolHalfExtent.x + xy.x, 0.0);
        float dEast  = max(poolHalfExtent.x - xy.x, 0.0);
        float dSouth = max(poolHalfExtent.y + xy.y, 0.0);
        float dNorth = max(poolHalfExtent.y - xy.y, 0.0);

        float wWest  = smoothstep(meniscusWidth, 0.0, dWest);
        float wEast  = smoothstep(meniscusWidth, 0.0, dEast);
        float wSouth = smoothstep(meniscusWidth, 0.0, dSouth);
        float wNorth = smoothstep(meniscusWidth, 0.0, dNorth);

        return vec2(wWest * wWest - wEast * wEast,
                    wSouth * wSouth - wNorth * wNorth) * meniscusStrength;
    }

    bool intersectPoolInterior(vec3 origin, vec3 dir, out vec3 hitPos, out float dist) {
        vec2 insetExtent = max(poolHalfExtent - vec2(1e-3), vec2(1e-3));
        vec3 p0 = vec3(clamp(origin.xy, -insetExtent, insetExtent), max(origin.z, bottomZ + 1e-3));

        float tMin = 1e9;
        if (dir.z < -1e-5) {
            float t = (bottomZ - p0.z) / dir.z;
            if (t > 0.0 && t < tMin) {
                tMin = t;
            }
        }
        if (dir.x < -1e-5) {
            float t = (-poolHalfExtent.x - p0.x) / dir.x;
            if (t > 0.0 && t < tMin) {
                tMin = t;
            }
        } else if (dir.x > 1e-5) {
            float t = (poolHalfExtent.x - p0.x) / dir.x;
            if (t > 0.0 && t < tMin) {
                tMin = t;
            }
        }
        if (dir.y < -1e-5) {
            float t = (-poolHalfExtent.y - p0.y) / dir.y;
            if (t > 0.0 && t < tMin) {
                tMin = t;
            }
        } else if (dir.y > 1e-5) {
            float t = (poolHalfExtent.y - p0.y) / dir.y;
            if (t > 0.0 && t < tMin) {
                tMin = t;
            }
        }

        if (tMin >= 1e8) {
            hitPos = p0;
            dist = max(p0.z - bottomZ, 0.0);
            return false;
        }

        hitPos = p0 + dir * tMin;
        dist = tMin;
        return true;
    }

    vec3 sampleSky(vec3 dir) {
        vec3 upDir = normalize(vec3(dir.xy, max(dir.z, 0.02)));
        float skyFactor = clamp(upDir.z * 0.5 + 0.5, 0.0, 1.0);
        vec3 sky = mix(vec3(0.24, 0.38, 0.50), vec3(0.42, 0.58, 0.72), skyFactor);
        vec3 sunDir = normalize(lightPos - FragPos);
        float sunGlow = pow(max(dot(upDir, sunDir), 0.0), 18.0);
        sky += lightColor * lightIntensity * sunGlow * 0.18;
        return sky;
    }

    vec3 evaluateReflection(vec3 origin, vec3 reflDir) {
        vec3 skyColor = sampleSky(reflDir);

        vec2 insetExtent = max(poolHalfExtent - vec2(1e-3), vec2(1e-3));
        vec3 p0 = vec3(clamp(origin.xy, -insetExtent, insetExtent), origin.z);

        float tWall = 1e9;
        vec3 wallNormal = vec3(0.0, 0.0, 1.0);

        if (reflDir.x < -1e-5) {
            float t = (-poolHalfExtent.x - p0.x) / reflDir.x;
            if (t > 0.0 && t < tWall) {
                tWall = t;
                wallNormal = vec3(1.0, 0.0, 0.0);
            }
        } else if (reflDir.x > 1e-5) {
            float t = (poolHalfExtent.x - p0.x) / reflDir.x;
            if (t > 0.0 && t < tWall) {
                tWall = t;
                wallNormal = vec3(-1.0, 0.0, 0.0);
            }
        }
        if (reflDir.y < -1e-5) {
            float t = (-poolHalfExtent.y - p0.y) / reflDir.y;
            if (t > 0.0 && t < tWall) {
                tWall = t;
                wallNormal = vec3(0.0, 1.0, 0.0);
            }
        } else if (reflDir.y > 1e-5) {
            float t = (poolHalfExtent.y - p0.y) / reflDir.y;
            if (t > 0.0 && t < tWall) {
                tWall = t;
                wallNormal = vec3(0.0, -1.0, 0.0);
            }
        }

        if (tWall < 1e8) {
            vec3 hit = p0 + reflDir * tWall;
            if (hit.z <= lipZ && hit.z >= p0.z - 0.5) {
                float band = fract(hit.z * 0.15);
                vec3 stone = mix(vec3(0.45, 0.48, 0.46), vec3(0.28, 0.34, 0.36), band);
                vec3 toLight = normalize(lightPos - hit);
                float ndotlWall = max(dot(wallNormal, toLight), 0.0);
                float heightFade = smoothstep(p0.z - 0.5, lipZ, hit.z);
                vec3 wallColor = stone * (0.48 + 0.58 * ndotlWall) * mix(0.86, 1.04, heightFade) * lightColor * lightIntensity;
                float lipBlend = smoothstep(lipZ - 0.6, lipZ, hit.z);
                return mix(wallColor, skyColor, lipBlend);
            }
        }

        return skyColor;
    }

    vec3 sampleRefractedRay(vec3 origin,
                            vec3 incident,
                            vec3 N,
                            float ior,
                            vec2 screenUV,
                            float waterLinDepth,
                            vec3 fallbackColor,
                            out float outDist) {
        float eta = clamp(airIOR / max(ior, 0.001), 0.1, 1.0);
        vec3 T = refract(incident, N, eta);
        if (dot(T, T) < 1e-6) {
            T = incident;
        } else {
            T = normalize(T);
        }

        vec3 hitPos;
        bool hitOk = intersectPoolInterior(origin, T, hitPos, outDist);
        if (!hitOk || hitPos.z > lipZ) {
            return fallbackColor;
        }

        vec4 clip = viewProj * vec4(hitPos, 1.0);
        if (clip.w <= 1e-4) {
            return fallbackColor;
        }
        vec2 refractUV = (clip.xy / clip.w) * 0.5 + 0.5;

        // Smoothly taper UV displacement near viewport edges so rays leaving the frustum never jump
        vec2 uvOffset = refractUV - screenUV;
        float fadeX = (uvOffset.x < 0.0) ? smoothstep(0.0, 0.14, screenUV.x)
                                         : smoothstep(0.0, 0.14, 1.0 - screenUV.x);
        float fadeY = (uvOffset.y < 0.0) ? smoothstep(0.0, 0.18, screenUV.y)
                                         : smoothstep(0.0, 0.18, 1.0 - screenUV.y);
        vec2 safeUV = clamp(screenUV + uvOffset * vec2(fadeX, fadeY), vec2(0.0005), vec2(0.9995));

        float sampleDepthRaw = texture(sceneDepthTex, safeUV).r;
        float sceneLinDepth = linearizeDepth(sampleDepthRaw);

        // Reject dry foreground walls or closer geometry in front of the water surface
        float depthMargin = sceneLinDepth - waterLinDepth;
        float validDepth = smoothstep(0.0, 0.65, depthMargin);
        float notSky = 1.0 - smoothstep(0.9994, 0.99999, sampleDepthRaw);
        float valid = validDepth * notSky;

        vec3 refracted = texture(sceneColorTex, safeUV).rgb;
        return mix(fallbackColor, refracted, valid);
    }

    vec3 softHighlightRollOff(vec3 c) {
        vec3 excess = max(c - vec3(0.86), vec3(0.0));
        return min(c, vec3(0.86)) + excess / (vec3(1.0) + excess * 7.5);
    }

    void main() {
        vec2 clampedXY = clamp(WaterXY, -poolHalfExtent, poolHalfExtent);
        float hFrag = 0.0;
        vec2 gradFrag = vec2(0.0);
        sampleWaterHeightAndGrad(clampedXY, hFrag, gradFrag);

        vec2 waveSlope = gradFrag * heightScale;
        float slopeLen = length(waveSlope);
        if (slopeLen > 2.5) {
            waveSlope *= 2.5 / slopeLen;
        }
        vec2 meniscusTilt = computeMeniscusTilt(clampedXY);
        vec3 N = normalize(vec3(-waveSlope.x + meniscusTilt.x, -waveSlope.y + meniscusTilt.y, 1.0));
        if (N.z < 0.0) {
            N = -N;
        }

        vec3 I = normalize(FragPos - viewPos);
        vec3 V = -I;
        // Keep wave normal view-facing on steep crests so reflections stay above the water surface
        float rawNdotV = dot(N, V);
        if (rawNdotV < 0.04) {
            N = normalize(N + V * (0.04 - rawNdotV));
        }
        float ndotv = max(dot(N, V), 0.0);

        // Physical Schlick Fresnel (air -> water)
        float r0 = (airIOR - waterIOR) / max(airIOR + waterIOR, 0.001);
        float F0 = r0 * r0;
        float fresnel = F0 + (1.0 - F0) * pow(1.0 - ndotv, 5.0);

        // Analytic reflection of above-water dry pool walls + sky gradient
        vec3 reflDir = reflect(-V, N);
        vec3 reflColor = evaluateReflection(FragPos, reflDir);

        // Crisp GGX sun specular highlight
        vec3 L = normalize(lightPos - FragPos);
        vec3 H = normalize(L + V);
        float ndotl = max(dot(N, L), 0.0);
        float ndoth = max(dot(N, H), 0.0);
        float vdoth = max(dot(V, H), 0.0);
        float rough = 0.16;
        float a = rough * rough;
        float a2 = a * a;
        float denom = ndoth * ndoth * (a2 - 1.0) + 1.0;
        float D = a2 / max(3.14159265 * denom * denom, 1e-5);
        float k = (rough + 1.0) * (rough + 1.0) * 0.125;
        float gv = ndotv / max(ndotv * (1.0 - k) + k, 1e-4);
        float gl = ndotl / max(ndotl * (1.0 - k) + k, 1e-4);
        float Fh = F0 + (1.0 - F0) * pow(1.0 - vdoth, 5.0);
        float specFactor = (D * gv * gl * Fh) / max(4.0 * ndotv * ndotl + 1e-4, 1e-4);
        vec3 sunSpec = lightColor * lightIntensity * ndotl * min(specFactor * 0.65, 1.6);

        // Beer-Lambert absorption coefficients modulated by turbidity
        float safeTurbidity = max(turbidity, 0.0);
        vec3 absorbCoeff = vec3(0.015, 0.0045, 0.003) * (1.0 + safeTurbidity * 2.5);
        vec3 scatterTint = vec3(0.025, 0.16, 0.21) * lightColor * lightIntensity;
        float scatterStrength = clamp(0.18 + safeTurbidity * 0.35, 0.0, 0.65);

        if (hasSceneTargets != 0) {
            vec2 screenUV = clamp(gl_FragCoord.xy / max(sceneResolution, vec2(1.0)), vec2(0.0005), vec2(0.9995));
            float waterLinDepth = linearizeDepth(gl_FragCoord.z);
            vec3 fallbackColor = texture(sceneColorTex, screenUV).rgb;

            float underwaterDist = 0.0;
            vec3 refractedColor = fallbackColor;
            if (dispersion > 0.0005) {
                float dR = 0.0;
                float dG = 0.0;
                float dB = 0.0;
                float iorR = max(waterIOR - dispersion, airIOR + 0.01);
                float iorG = max(waterIOR, airIOR + 0.01);
                float iorB = max(waterIOR + dispersion, airIOR + 0.01);
                refractedColor.r = sampleRefractedRay(FragPos, I, N, iorR, screenUV, waterLinDepth, fallbackColor, dR).r;
                refractedColor.g = sampleRefractedRay(FragPos, I, N, iorG, screenUV, waterLinDepth, fallbackColor, dG).g;
                refractedColor.b = sampleRefractedRay(FragPos, I, N, iorB, screenUV, waterLinDepth, fallbackColor, dB).b;
                underwaterDist = dG;
            } else {
                refractedColor = sampleRefractedRay(FragPos, I, N, waterIOR, screenUV, waterLinDepth, fallbackColor, underwaterDist);
            }

            vec3 transmittance = exp(-max(underwaterDist, 0.0) * absorbCoeff);
            vec3 underwater = refractedColor * transmittance + scatterTint * (vec3(1.0) - transmittance) * scatterStrength;

            vec3 composed = mix(underwater, reflColor, fresnel) + sunSpec;
            composed = softHighlightRollOff(composed * max(exposure, 0.01));
            FragColor = vec4(clamp(composed, 0.0, 0.99), 1.0);
        } else {
            // Translucent physically-shaded fallback when SceneTargets are not bound
            float eta = clamp(airIOR / max(waterIOR, 0.001), 0.1, 1.0);
            vec3 T = refract(I, N, eta);
            if (dot(T, T) < 1e-6) {
                T = I;
            } else {
                T = normalize(T);
            }
            vec3 hitPos;
            float underwaterDist = 0.0;
            intersectPoolInterior(FragPos, T, hitPos, underwaterDist);

            vec3 transmittance = exp(-max(underwaterDist, 0.0) * absorbCoeff);
            float transAvg = dot(transmittance, vec3(0.3333333));
            float specLuma = max(sunSpec.r, max(sunSpec.g, sunSpec.b));
            float alpha = clamp(1.0 - (1.0 - fresnel) * transAvg + specLuma * 0.45, 0.14, 0.95);

            vec3 waterBody = mix(vec3(0.03, 0.22, 0.28), scatterTint, clamp(safeTurbidity * 0.5, 0.0, 0.7));
            vec3 surfacePremul = waterBody * (1.0 - fresnel) * (1.0 - transAvg) + reflColor * fresnel + sunSpec;
            vec3 color = softHighlightRollOff((surfacePremul / max(alpha, 0.08)) * max(exposure, 0.01));
            FragColor = vec4(clamp(color, 0.0, 0.99), alpha);
        }
    }
)GLSL";
