#pragma once

// One vertex shader for the water, the pool floor, and the caustic pass.
// The caustic fragment writes intensity in alpha. The floor reads that alpha
// with UV (FragPos.xy + 200) / 400, the pool from -200 to 200.

const char* surfaceVertexShaderSource = R"(
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
        Normal = mat3(transpose(inverse(model))) * aNormal;
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)";

const char* causticVertexShaderSource = R"(
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
        Normal = mat3(transpose(inverse(model))) * aNormal;
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)";

const char* causticVertexShaderSource = R"(
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
        Normal = mat3(transpose(inverse(model))) * aNormal;
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
    in vec3 FragPos;
    in vec3 Normal;

    out vec4 FragColor;

    uniform vec3 lightPos;
    uniform float bottomZ;
    uniform float waterIOR;
    uniform float airIOR;
    uniform float time;

    void main() {
        vec3 N = normalize(Normal);
        if (N.z < 0.0) {
            N = -N;
        }

        vec3 I = vec3(0.0, 0.0, -1.0);
        vec3 T = refract(I, N, airIOR / max(waterIOR, 0.001));
        float intensity = 0.0;
        if (T.z < -0.02) {
            float bend = length(T.xy);
            intensity = clamp(bend * 6.0, 0.0, 1.0) * max(N.z, 0.0);
        }

        FragColor = vec4(0.0, 0.0, 0.0, intensity);
    }
)";

const char* bottomFragmentShaderSource = R"(
    #version 330 core
    in vec3 FragPos;
    in vec3 Normal;

    out vec4 FragColor;

    uniform sampler2D causticsTexture;
    uniform float time;

    void main() {
        vec2 causticsUV = (FragPos.xy + vec2(200.0)) / 400.0;
        float causticIntensity = texture(causticsTexture, causticsUV).a;

        vec3 floorColor = vec3(0.06, 0.16, 0.22);
        vec3 warm = vec3(1.50, 1.18, 0.62);
        vec3 finalColor = floorColor + warm * causticIntensity;
        FragColor = vec4(clamp(finalColor, 0.0, 1.0), 1.0);
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
