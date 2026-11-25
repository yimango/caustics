#pragma once

#define WAVE_HEIGHT_SAMPLE R"(
    float catmullRom(float p0, float p1, float p2, float p3, float t) {
        float t2 = t * t;
        float t3 = t2 * t;
        return 0.5 * ((2.0 * p1) +
            (-p0 + p2) * t +
            (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
            (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3);
    }

    float tapHeight(float xIndex, float yIndex, vec2 lo, vec2 hi) {
        vec2 tc = vec2((yIndex + 0.5) / gridSize.y, (xIndex + 0.5) / gridSize.x);
        return texture(heightTex, clamp(tc, lo, hi)).r;
    }

    float sampleHeight(vec2 xy) {
        vec2 halfTexel = 0.5 / gridSize;
        vec2 uv = xy / (poolExtent * 2.0) + 0.5 + halfTexel;
        vec2 index = uv * gridSize - 0.5;
        vec2 cell = floor(index);
        vec2 t = index - cell;
        vec2 lo = vec2(halfTexel.y, halfTexel.x);
        vec2 hi = vec2(1.0) - lo;
        float x0 = cell.x - 1.0;
        float x1 = cell.x;
        float x2 = cell.x + 1.0;
        float x3 = cell.x + 2.0;
        float y0 = cell.y - 1.0;
        float y1 = cell.y;
        float y2 = cell.y + 1.0;
        float y3 = cell.y + 2.0;
        float r0 = catmullRom(
            tapHeight(x0, y0, lo, hi), tapHeight(x1, y0, lo, hi),
            tapHeight(x2, y0, lo, hi), tapHeight(x3, y0, lo, hi), t.x);
        float r1 = catmullRom(
            tapHeight(x0, y1, lo, hi), tapHeight(x1, y1, lo, hi),
            tapHeight(x2, y1, lo, hi), tapHeight(x3, y1, lo, hi), t.x);
        float r2 = catmullRom(
            tapHeight(x0, y2, lo, hi), tapHeight(x1, y2, lo, hi),
            tapHeight(x2, y2, lo, hi), tapHeight(x3, y2, lo, hi), t.x);
        float r3 = catmullRom(
            tapHeight(x0, y3, lo, hi), tapHeight(x1, y3, lo, hi),
            tapHeight(x2, y3, lo, hi), tapHeight(x3, y3, lo, hi), t.x);
        return catmullRom(r0, r1, r2, r3, t.y);
    }

    void heightSlope(vec2 xy, out float dhdx, out float dhdy) {
        float hxp = sampleHeight(xy + vec2(0.5, 0.0));
        float hxm = sampleHeight(xy - vec2(0.5, 0.0));
        float hyp = sampleHeight(xy + vec2(0.0, 0.5));
        float hym = sampleHeight(xy - vec2(0.0, 0.5));
        dhdx = (hxp - hxm) / 1.0;
        dhdy = (hyp - hym) / 1.0;
    }
)"

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
)" WAVE_HEIGHT_SAMPLE R"(
    void main() {
        vec2 xy = (model * vec4(aPos, 1.0)).xy;
        float h = sampleHeight(xy);
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
            float spacing = meshSpacing > 0.0 ? meshSpacing : poolExtent * 2.0 / gridSize.x;
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

#undef WAVE_HEIGHT_SAMPLE

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

const char* causticBlurVertexShaderSource = R"(
    #version 330 core
    void main() {
        vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
        gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
    }
)";

const char* causticBlurFragmentShaderSource = R"(
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
