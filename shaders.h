#pragma once

const char* surfaceVertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;

    out vec3 FragPos;
    out vec3 Normal;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    uniform sampler2D heightTex;
    uniform float heightScale;
    uniform float poolExtent;
    uniform vec2 gridSize;

    vec2 heightCoord(vec2 xy) {
        vec2 halfTexel = 0.5 / gridSize;
        vec2 uv = clamp(xy / (poolExtent * 2.0) + 0.5 + halfTexel, halfTexel, vec2(1.0) - halfTexel);
        return vec2(uv.y, uv.x);
    }

    void heightSlope(vec2 xy, out float dhdx, out float dhdy) {
        vec2 halfTexel = 0.5 / gridSize;
        vec2 uv = clamp(xy / (poolExtent * 2.0) + 0.5 + halfTexel, halfTexel, vec2(1.0) - halfTexel);
        vec2 texel = 1.0 / gridSize;
        vec2 tc = vec2(uv.y, uv.x);
        vec2 lo = vec2(halfTexel.y, halfTexel.x);
        vec2 hi = vec2(1.0) - lo;
        float hxp = texture(heightTex, clamp(tc + vec2(0.0, texel.x), lo, hi)).r;
        float hxm = texture(heightTex, clamp(tc - vec2(0.0, texel.x), lo, hi)).r;
        float hyp = texture(heightTex, clamp(tc + vec2(texel.y, 0.0), lo, hi)).r;
        float hym = texture(heightTex, clamp(tc - vec2(texel.y, 0.0), lo, hi)).r;
        float spacing = poolExtent * 2.0 / gridSize.x;
        dhdx = (hxp - hxm) / (2.0 * spacing);
        dhdy = (hyp - hym) / (2.0 * spacing);
    }

    void main() {
        vec2 xy = (model * vec4(aPos, 1.0)).xy;
        float h = texture(heightTex, heightCoord(xy)).r;
        float dhdx;
        float dhdy;
        heightSlope(xy, dhdx, dhdy);
        FragPos = vec3(xy, h * heightScale);
        Normal = normalize(vec3(-dhdx * heightScale, -dhdy * heightScale, 1.0));
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)";

const char* causticVertexShaderSource = R"(
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
    uniform float poolExtent;
    uniform sampler2D heightTex;
    uniform vec2 gridSize;

    vec2 heightCoord(vec2 xy) {
        vec2 halfTexel = 0.5 / gridSize;
        vec2 uv = clamp(xy / (poolExtent * 2.0) + 0.5 + halfTexel, halfTexel, vec2(1.0) - halfTexel);
        return vec2(uv.y, uv.x);
    }

    float sampleHeight(vec2 xy) {
        return texture(heightTex, heightCoord(xy)).r;
    }

    void heightSlope(vec2 xy, out float dhdx, out float dhdy) {
        vec2 halfTexel = 0.5 / gridSize;
        vec2 uv = clamp(xy / (poolExtent * 2.0) + 0.5 + halfTexel, halfTexel, vec2(1.0) - halfTexel);
        vec2 texel = 1.0 / gridSize;
        vec2 tc = vec2(uv.y, uv.x);
        vec2 lo = vec2(halfTexel.y, halfTexel.x);
        vec2 hi = vec2(1.0) - lo;
        float hxp = texture(heightTex, clamp(tc + vec2(0.0, texel.x), lo, hi)).r;
        float hxm = texture(heightTex, clamp(tc - vec2(0.0, texel.x), lo, hi)).r;
        float hyp = texture(heightTex, clamp(tc + vec2(texel.y, 0.0), lo, hi)).r;
        float hym = texture(heightTex, clamp(tc - vec2(texel.y, 0.0), lo, hi)).r;
        float spacing = poolExtent * 2.0 / gridSize.x;
        dhdx = (hxp - hxm) / (2.0 * spacing);
        dhdy = (hyp - hym) / (2.0 * spacing);
    }

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
            float spacing = poolExtent * 2.0 / gridSize.x;
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

const char* bottomVertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;

    out vec3 FragPos;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main() {
        FragPos = vec3(model * vec4(aPos, 1.0));
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)";

const char* skyboxVertexShaderSource = R"(
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

const char* skyboxFragmentShaderSource = R"(
    #version 330 core
    in vec3 TexCoords;

    out vec4 FragColor;

    void main() {
        float skyFactor = normalize(TexCoords).z * 0.5 + 0.5;
        vec3 skyColor = mix(vec3(0.20, 0.34, 0.46), vec3(0.38, 0.54, 0.68), skyFactor);
        FragColor = vec4(skyColor, 1.0);
    }
)";

const char* causticsFragmentShaderSource = R"(
    #version 330 core
    smooth in float intensity;

    out vec4 FragColor;

    void main() {
        FragColor = vec4(intensity);
    }
)";

const char* bottomFragmentShaderSource = R"(
    #version 330 core
    in vec3 FragPos;

    out vec4 FragColor;

    uniform sampler2D causticsTexture;
    uniform float poolExtent;
    uniform float bottomZ;

    void main() {
        if (FragPos.z > bottomZ + 0.5) {
            float band = fract(FragPos.z * 0.15);
            vec3 stone = mix(vec3(0.45, 0.48, 0.46), vec3(0.28, 0.34, 0.36), band);
            FragColor = vec4(stone, 1.0);
            return;
        }
        vec2 causticsUV = (FragPos.xy + vec2(poolExtent)) / (poolExtent * 2.0);
        vec2 texel = vec2(1.0 / 2048.0);
        float center = texture(causticsTexture, causticsUV).r;
        float left = texture(causticsTexture, causticsUV + vec2(-texel.x, 0.0)).r;
        float right = texture(causticsTexture, causticsUV + vec2(texel.x, 0.0)).r;
        float down = texture(causticsTexture, causticsUV + vec2(0.0, -texel.y)).r;
        float up = texture(causticsTexture, causticsUV + vec2(0.0, texel.y)).r;
        float light = center * 0.5 + (left + right + down + up) * 0.125;
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

const char* waterFragmentShaderSource = R"(
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

const char* overlayVertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec2 aPos;

    uniform vec2 viewport;

    void main() {
        vec2 ndc = vec2(aPos.x / viewport.x * 2.0 - 1.0, aPos.y / viewport.y * 2.0 - 1.0);
        gl_Position = vec4(ndc, 0.0, 1.0);
    }
)";

const char* overlayFragmentShaderSource = R"(
    #version 330 core
    uniform vec3 color;
    uniform float alpha;

    out vec4 FragColor;

    void main() {
        FragColor = vec4(color, alpha);
    }
)";
