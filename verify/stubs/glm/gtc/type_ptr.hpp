#pragma once
#include <glm/glm.hpp>
namespace glm {
inline const float* value_ptr(const mat4& m) { return m.m; }
inline const float* value_ptr(const vec3& v) { return &v.x; }
}
