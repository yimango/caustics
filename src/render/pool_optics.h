#pragma once

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

#include "render/pool_coords.h"

enum PoolFace : int {
    POOL_FACE_NONE = -1,
    POOL_FACE_FLOOR = 0, // z = bottomZ, inward normal (0, 0, +1)
    POOL_FACE_SOUTH = 1, // y = -hh,     inward normal (0, +1, 0)
    POOL_FACE_EAST = 2,  // x = +hw,     inward normal (-1, 0, 0)
    POOL_FACE_NORTH = 3, // y = +hh,     inward normal (0, -1, 0)
    POOL_FACE_WEST = 4,  // x = -hw,     inward normal (+1, 0, 0)
};

struct PoolBoxHit {
    bool hit = false;
    int face = POOL_FACE_NONE;
    float t = 0.0f;
    glm::vec3 pos = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f, 0.0f, 1.0f);
};

inline glm::vec2 poolAtlasHalfSpan(const glm::vec2& halfExtent, float bottomZ, float lipZ) {
    const float wallHeight = std::max(lipZ - bottomZ, 0.0f);
    return halfExtent + glm::vec2(wallHeight, wallHeight);
}

inline glm::vec2 poolAtlasSpan(const glm::vec2& halfExtent, float bottomZ, float lipZ) {
    return poolAtlasHalfSpan(halfExtent, bottomZ, lipZ) * 2.0f;
}

inline PoolBoxHit poolBoxHit(const glm::vec3& origin,
                             const glm::vec3& dir,
                             const glm::vec2& halfExtent,
                             float bottomZ,
                             float lipZ) {
    PoolBoxHit result;
    const float hw = halfExtent.x;
    const float hh = halfExtent.y;
    const float eps = 1e-6f;
    const float boundEps = 1e-3f;

    float bestT = 1e30f;
    int bestFace = POOL_FACE_NONE;

    if (dir.z < -eps) {
        const float t = (bottomZ - origin.z) / dir.z;
        if (t >= -boundEps && t < bestT) {
            bestT = std::max(0.0f, t);
            bestFace = POOL_FACE_FLOOR;
        }
    }
    if (dir.y < -eps) {
        const float t = (-hh - origin.y) / dir.y;
        if (t >= -boundEps && t < bestT) {
            bestT = std::max(0.0f, t);
            bestFace = POOL_FACE_SOUTH;
        }
    }
    if (dir.x > eps) {
        const float t = (hw - origin.x) / dir.x;
        if (t >= -boundEps && t < bestT) {
            bestT = std::max(0.0f, t);
            bestFace = POOL_FACE_EAST;
        }
    }
    if (dir.y > eps) {
        const float t = (hh - origin.y) / dir.y;
        if (t >= -boundEps && t < bestT) {
            bestT = std::max(0.0f, t);
            bestFace = POOL_FACE_NORTH;
        }
    }
    if (dir.x < -eps) {
        const float t = (-hw - origin.x) / dir.x;
        if (t >= -boundEps && t < bestT) {
            bestT = std::max(0.0f, t);
            bestFace = POOL_FACE_WEST;
        }
    }

    if (bestFace == POOL_FACE_NONE) {
        return result;
    }

    glm::vec3 pos = origin + dir * bestT;
    glm::vec3 normal(0.0f, 0.0f, 1.0f);
    switch (bestFace) {
        case POOL_FACE_FLOOR:
            pos.z = bottomZ;
            normal = glm::vec3(0.0f, 0.0f, 1.0f);
            break;
        case POOL_FACE_SOUTH:
            pos.y = -hh;
            normal = glm::vec3(0.0f, 1.0f, 0.0f);
            break;
        case POOL_FACE_EAST:
            pos.x = hw;
            normal = glm::vec3(-1.0f, 0.0f, 0.0f);
            break;
        case POOL_FACE_NORTH:
            pos.y = hh;
            normal = glm::vec3(0.0f, -1.0f, 0.0f);
            break;
        case POOL_FACE_WEST:
            pos.x = -hw;
            normal = glm::vec3(1.0f, 0.0f, 0.0f);
            break;
        default:
            return result;
    }

    if (pos.z < bottomZ - boundEps || pos.z > lipZ + boundEps ||
        std::abs(pos.x) > hw + boundEps || std::abs(pos.y) > hh + boundEps) {
        return result;
    }

    pos.x = std::clamp(pos.x, -hw, hw);
    pos.y = std::clamp(pos.y, -hh, hh);
    pos.z = std::clamp(pos.z, bottomZ, lipZ);

    result.hit = true;
    result.face = bestFace;
    result.t = bestT;
    result.pos = pos;
    result.normal = normal;
    return result;
}

inline glm::vec2 unfoldPoolPoint(const glm::vec3& pos,
                                 int face,
                                 const glm::vec2& halfExtent,
                                 float bottomZ) {
    const float dz = pos.z - bottomZ;
    switch (face) {
        case POOL_FACE_SOUTH:
            return glm::vec2(pos.x, -halfExtent.y - dz);
        case POOL_FACE_EAST:
            return glm::vec2(halfExtent.x + dz, pos.y);
        case POOL_FACE_NORTH:
            return glm::vec2(pos.x, halfExtent.y + dz);
        case POOL_FACE_WEST:
            return glm::vec2(-halfExtent.x - dz, pos.y);
        case POOL_FACE_FLOOR:
        default:
            return glm::vec2(pos.x, pos.y);
    }
}

inline glm::vec2 poolUnfoldedToUV(const glm::vec2& unfolded,
                                  const glm::vec2& halfExtent,
                                  float bottomZ,
                                  float lipZ) {
    const glm::vec2 halfSpan = poolAtlasHalfSpan(halfExtent, bottomZ, lipZ);
    return (unfolded + halfSpan) / (halfSpan * 2.0f);
}

inline glm::vec2 poolPointToAtlasUV(const glm::vec3& pos,
                                    int face,
                                    const glm::vec2& halfExtent,
                                    float bottomZ,
                                    float lipZ) {
    return poolUnfoldedToUV(unfoldPoolPoint(pos, face, halfExtent, bottomZ), halfExtent, bottomZ, lipZ);
}

inline float rimSmoothstep(float edge0, float edge1, float x) {
    if (edge1 <= edge0) {
        return (x >= edge1) ? 1.0f : 0.0f;
    }
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float rimVisibility(const glm::vec3& pos,
                           const glm::vec3& lightPos,
                           const glm::vec2& halfExtent,
                           float lipZ,
                           float penumbra = 1.5f) {
    const glm::vec3 d = lightPos - pos;
    const float eps = 1e-6f;
    const float boundEps = 1e-4f;
    float tExit = 1e30f;
    bool hasExit = false;

    if (d.x > eps) {
        const float t = (halfExtent.x - pos.x) / d.x;
        if (t >= -boundEps && t < tExit) {
            tExit = std::max(0.0f, t);
            hasExit = true;
        }
    } else if (d.x < -eps) {
        const float t = (-halfExtent.x - pos.x) / d.x;
        if (t >= -boundEps && t < tExit) {
            tExit = std::max(0.0f, t);
            hasExit = true;
        }
    }

    if (d.y > eps) {
        const float t = (halfExtent.y - pos.y) / d.y;
        if (t >= -boundEps && t < tExit) {
            tExit = std::max(0.0f, t);
            hasExit = true;
        }
    } else if (d.y < -eps) {
        const float t = (-halfExtent.y - pos.y) / d.y;
        if (t >= -boundEps && t < tExit) {
            tExit = std::max(0.0f, t);
            hasExit = true;
        }
    }

    if (!hasExit || tExit >= 1.0f) {
        return 1.0f;
    }

    const float zExit = pos.z + tExit * d.z;
    if (penumbra <= 0.0f) {
        return (zExit >= lipZ) ? 1.0f : 0.0f;
    }
    return rimSmoothstep(lipZ - penumbra, lipZ + penumbra, zExit);
}

// Shared GLSL mirror of the CPU optical geometry helpers above.
#define POOL_OPTICS_GLSL R"(
    const int POOL_FACE_NONE = -1;
    const int POOL_FACE_FLOOR = 0;
    const int POOL_FACE_SOUTH = 1;
    const int POOL_FACE_EAST = 2;
    const int POOL_FACE_NORTH = 3;
    const int POOL_FACE_WEST = 4;

    bool poolBoxHit(vec3 origin, vec3 dir, vec2 halfExt, float botZ, float topZ, out vec3 hitPos, out int hitFace) {
        float hw = halfExt.x;
        float hh = halfExt.y;
        float eps = 1e-6;
        float boundEps = 1e-3;

        float bestT = 1e20;
        int bestFace = POOL_FACE_NONE;

        if (dir.z < -eps) {
            float t = (botZ - origin.z) / dir.z;
            if (t >= -boundEps && t < bestT) {
                bestT = max(0.0, t);
                bestFace = POOL_FACE_FLOOR;
            }
        }
        if (dir.y < -eps) {
            float t = (-hh - origin.y) / dir.y;
            if (t >= -boundEps && t < bestT) {
                bestT = max(0.0, t);
                bestFace = POOL_FACE_SOUTH;
            }
        }
        if (dir.x > eps) {
            float t = (hw - origin.x) / dir.x;
            if (t >= -boundEps && t < bestT) {
                bestT = max(0.0, t);
                bestFace = POOL_FACE_EAST;
            }
        }
        if (dir.y > eps) {
            float t = (hh - origin.y) / dir.y;
            if (t >= -boundEps && t < bestT) {
                bestT = max(0.0, t);
                bestFace = POOL_FACE_NORTH;
            }
        }
        if (dir.x < -eps) {
            float t = (-hw - origin.x) / dir.x;
            if (t >= -boundEps && t < bestT) {
                bestT = max(0.0, t);
                bestFace = POOL_FACE_WEST;
            }
        }

        if (bestFace == POOL_FACE_NONE) {
            hitPos = origin;
            hitFace = POOL_FACE_NONE;
            return false;
        }

        vec3 p = origin + dir * bestT;
        if (bestFace == POOL_FACE_FLOOR) {
            p.z = botZ;
        } else if (bestFace == POOL_FACE_SOUTH) {
            p.y = -hh;
        } else if (bestFace == POOL_FACE_EAST) {
            p.x = hw;
        } else if (bestFace == POOL_FACE_NORTH) {
            p.y = hh;
        } else if (bestFace == POOL_FACE_WEST) {
            p.x = -hw;
        }

        if (p.z < botZ - boundEps || p.z > topZ + boundEps ||
            abs(p.x) > hw + boundEps || abs(p.y) > hh + boundEps) {
            hitPos = origin;
            hitFace = POOL_FACE_NONE;
            return false;
        }

        p.x = clamp(p.x, -hw, hw);
        p.y = clamp(p.y, -hh, hh);
        p.z = clamp(p.z, botZ, topZ);
        hitPos = p;
        hitFace = bestFace;
        return true;
    }

    vec2 unfoldPoolPoint(vec3 pos, int face, vec2 halfExt, float botZ) {
        float dz = pos.z - botZ;
        if (face == POOL_FACE_SOUTH) {
            return vec2(pos.x, -halfExt.y - dz);
        }
        if (face == POOL_FACE_EAST) {
            return vec2(halfExt.x + dz, pos.y);
        }
        if (face == POOL_FACE_NORTH) {
            return vec2(pos.x, halfExt.y + dz);
        }
        if (face == POOL_FACE_WEST) {
            return vec2(-halfExt.x - dz, pos.y);
        }
        return pos.xy;
    }

    vec2 poolPointToAtlasUV(vec3 pos, int face, vec2 halfExt, float botZ, float topZ) {
        float wallH = max(topZ - botZ, 0.0);
        vec2 halfSpan = halfExt + vec2(wallH);
        vec2 u = unfoldPoolPoint(pos, face, halfExt, botZ);
        return (u + halfSpan) / (halfSpan * 2.0);
    }

    vec4 poolWallMask(int face) {
        if (face == POOL_FACE_SOUTH) return vec4(1.0, 0.0, 0.0, 0.0);
        if (face == POOL_FACE_EAST)  return vec4(0.0, 1.0, 0.0, 0.0);
        if (face == POOL_FACE_NORTH) return vec4(0.0, 0.0, 1.0, 0.0);
        if (face == POOL_FACE_WEST)  return vec4(0.0, 0.0, 0.0, 1.0);
        return vec4(0.0);
    }

    float rimVisibility(vec3 pos, vec3 lPos, vec2 halfExt, float topZ, float penumbra) {
        vec3 d = lPos - pos;
        float eps = 1e-6;
        float boundEps = 1e-4;
        float tExit = 1e20;
        bool hasExit = false;

        if (d.x > eps) {
            float t = (halfExt.x - pos.x) / d.x;
            if (t >= -boundEps && t < tExit) {
                tExit = max(0.0, t);
                hasExit = true;
            }
        } else if (d.x < -eps) {
            float t = (-halfExt.x - pos.x) / d.x;
            if (t >= -boundEps && t < tExit) {
                tExit = max(0.0, t);
                hasExit = true;
            }
        }

        if (d.y > eps) {
            float t = (halfExt.y - pos.y) / d.y;
            if (t >= -boundEps && t < tExit) {
                tExit = max(0.0, t);
                hasExit = true;
            }
        } else if (d.y < -eps) {
            float t = (-halfExt.y - pos.y) / d.y;
            if (t >= -boundEps && t < tExit) {
                tExit = max(0.0, t);
                hasExit = true;
            }
        }

        if (!hasExit || tExit >= 1.0) {
            return 1.0;
        }

        float zExit = pos.z + tExit * d.z;
        return smoothstep(topZ - penumbra, topZ + penumbra, zExit);
    }
)"
