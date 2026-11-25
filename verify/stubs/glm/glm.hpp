#pragma once
namespace glm {
struct vec3 {
    float x, y, z;
    vec3() : x(0), y(0), z(0) {}
    vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    vec3 operator-() const { return vec3(-x, -y, -z); }
    vec3 operator-(const vec3& o) const { return vec3(x - o.x, y - o.y, z - o.z); }
    vec3 operator+(const vec3& o) const { return vec3(x + o.x, y + o.y, z + o.z); }
    vec3 operator*(float s) const { return vec3(x * s, y * s, z * s); }
};
struct vec4 {
    float x, y, z, w;
    vec4() : x(0), y(0), z(0), w(0) {}
    vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
};
struct mat4;
struct mat3 {
    float m[9]{};
    mat3() = default;
    explicit mat3(const mat4&);
};
struct mat4 {
    float m[16]{};
    mat4() = default;
    explicit mat4(float) {}
    explicit mat4(const mat3&) {}
};
inline vec3 normalize(const vec3& v) { return v; }
inline float dot(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length(const vec3& v) { return v.x; }
}
