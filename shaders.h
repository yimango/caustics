#pragma once

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
    out float casts;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    uniform vec3 lightPos;
    uniform float bottomZ;
    uniform float waterIOR;
    uniform float airIOR;

    void main() {
        vec3 P = (model * vec4(aPos, 1.0)).xyz;
        mat3 normalMatrix = mat3(transpose(inverse(model)));
        vec3 N = normalize(normalMatrix * aNormal);
        if (N.z < 0.0) {
            N = -N;
        }
        vec3 optical = normalize(vec3(N.xy / max(N.z, 0.0001), 1.0));

        vec3 origin = vec3(P.xy, P.z / 8.0);
        vec3 I = normalize(origin - lightPos);
        float eta = airIOR / max(waterIOR, 0.001);
        vec3 T = refract(I, optical, eta);

        casts = 0.0;
        vec3 hit = origin;
        if (T.z < -0.02) {
            float t = (bottomZ - origin.z) / T.z;
            if (t > 0.0) {
                hit = origin + T * t;
                casts = 1.0;
            }
        }

        if (casts < 0.5) {
            gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        } else {
            gl_Position = projection * view * vec4(hit, 1.0);
        }

        FragPos = P;
        Normal = N;
    }
)";

const char* causticGeometryShaderSource = R"(
    #version 330 core
    layout(triangles) in;
    layout(triangle_strip, max_vertices = 3) out;

    in vec3 FragPos[];
    in float casts[];
    flat out float vIntensity;

    void main() {
        if (casts[0] < 0.5 || casts[1] < 0.5 || casts[2] < 0.5) {
            return;
        }

        vec2 a = FragPos[0].xy;
        vec2 b = FragPos[1].xy;
        vec2 c = FragPos[2].xy;
        float source = abs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y));

        vec2 h0 = gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w * 200.0;
        vec2 h1 = gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w * 200.0;
        vec2 h2 = gl_in[2].gl_Position.xy / gl_in[2].gl_Position.w * 200.0;
        float receiver = abs((h1.x - h0.x) * (h2.y - h0.y) - (h2.x - h0.x) * (h1.y - h0.y));
        vIntensity = clamp(source / max(receiver, 0.0001), 0.0, 6.0);

        for (int i = 0; i < 3; ++i) {
            gl_Position = gl_in[i].gl_Position;
            EmitVertex();
        }
        EndPrimitive();
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
    flat in float vIntensity;

    out vec4 FragColor;

    void main() {
        FragColor = vec4(vIntensity);
    }
)";

const char* bottomFragmentShaderSource = R"(
    #version 330 core
    in vec3 FragPos;

    out vec4 FragColor;

    uniform sampler2D causticsTexture;

    void main() {
        vec2 causticsUV = (FragPos.xy + vec2(200.0)) / 400.0;
        float causticIntensity = texture(causticsTexture, causticsUV).a;
        float light = causticIntensity;
        vec3 floorColor = vec3(0.05, 0.14, 0.20);
        vec3 lit = floorColor * (0.25 + light);
        vec3 hot = vec3(1.45, 1.15, 0.62) * max(light - 1.0, 0.0);
        FragColor = vec4(clamp(lit + hot, 0.0, 1.0), 1.0);
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
