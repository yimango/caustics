#pragma once
#include <glm/glm.hpp>
namespace glm {
inline mat4 lookAt(const vec3&, const vec3&, const vec3&) { return mat4(1.0f); }
inline mat4 perspective(float, float, float, float) { return mat4(1.0f); }
inline mat4 ortho(float, float, float, float, float, float) { return mat4(1.0f); }
inline float radians(float degrees) { return degrees; }
inline vec3 unProject(const vec3&, const mat4&, const mat4&, const vec4&) { return vec3(); }
inline mat3::mat3(const mat4&) {}
}
