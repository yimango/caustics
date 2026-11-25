#pragma once
#include <glm/glm.hpp>
namespace glm {
inline mat4 lookAt(const vec3&, const vec3&, const vec3&) { return mat4(1.0f); }
inline mat4 perspective(float, float, float, float) { return mat4(1.0f); }
inline mat4 ortho(float, float, float, float, float, float) { return mat4(1.0f); }
inline float radians(float degrees) { return degrees; }
inline mat3::mat3(const mat4&) {}
}
