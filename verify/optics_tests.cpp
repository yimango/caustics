#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

#include <glm/glm.hpp>
#include "render/pool_coords.h"

#if __has_include("render/pool_optics.h")
#include "render/pool_optics.h"
#define HAS_REAL_POOL_OPTICS 1
#else
#define HAS_REAL_POOL_OPTICS 0

// Self-contained reference fallback matching Track B's src/render/pool_optics.h API
// so optics_tests compiles and passes standalone before Track B merges.
enum PoolFace : int {
    POOL_FACE_NONE = -1,
    POOL_FACE_FLOOR = 0,
    POOL_FACE_SOUTH = 1,
    POOL_FACE_EAST = 2,
    POOL_FACE_NORTH = 3,
    POOL_FACE_WEST = 4
};

struct PoolBoxHit {
    bool hit = false;
    int face = POOL_FACE_NONE;
    float t = 0.0f;
    glm::vec3 pos = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f, 0.0f, 1.0f);
};

inline glm::vec2 poolAtlasHalfSpan(const glm::vec2& halfExtent, float bottomZ, float lipZ) {
    const float wallH = std::max(0.0f, lipZ - bottomZ);
    return halfExtent + glm::vec2(wallH, wallH);
}

inline glm::vec2 poolAtlasSpan(const glm::vec2& halfExtent, float bottomZ, float lipZ) {
    return 2.0f * poolAtlasHalfSpan(halfExtent, bottomZ, lipZ);
}

inline PoolBoxHit poolBoxHit(const glm::vec3& origin,
                             const glm::vec3& dir,
                             const glm::vec2& halfExtent,
                             float bottomZ,
                             float lipZ) {
    PoolBoxHit best;
    float bestT = 1e30f;
    const float eps = 1e-4f;

    // Floor: z = bottomZ, inward normal (0, 0, +1)
    if (dir.z < -1e-6f) {
        const float t = (bottomZ - origin.z) / dir.z;
        if (t > 0.0f && t < bestT) {
            const glm::vec3 p = origin + dir * t;
            if (p.x >= -halfExtent.x - eps && p.x <= halfExtent.x + eps &&
                p.y >= -halfExtent.y - eps && p.y <= halfExtent.y + eps) {
                bestT = t;
                best.hit = true;
                best.face = POOL_FACE_FLOOR;
                best.t = t;
                best.pos = glm::vec3(p.x, p.y, bottomZ);
                best.normal = glm::vec3(0.0f, 0.0f, 1.0f);
            }
        }
    }

    // South wall: y = -halfExtent.y, inward normal (0, +1, 0)
    if (dir.y < -1e-6f) {
        const float t = (-halfExtent.y - origin.y) / dir.y;
        if (t > 0.0f && t < bestT) {
            const glm::vec3 p = origin + dir * t;
            if (p.x >= -halfExtent.x - eps && p.x <= halfExtent.x + eps &&
                p.z >= bottomZ - eps && p.z <= lipZ + eps) {
                bestT = t;
                best.hit = true;
                best.face = POOL_FACE_SOUTH;
                best.t = t;
                best.pos = glm::vec3(p.x, -halfExtent.y, p.z);
                best.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }
        }
    }

    // East wall: x = +halfExtent.x, inward normal (-1, 0, 0)
    if (dir.x > 1e-6f) {
        const float t = (halfExtent.x - origin.x) / dir.x;
        if (t > 0.0f && t < bestT) {
            const glm::vec3 p = origin + dir * t;
            if (p.y >= -halfExtent.y - eps && p.y <= halfExtent.y + eps &&
                p.z >= bottomZ - eps && p.z <= lipZ + eps) {
                bestT = t;
                best.hit = true;
                best.face = POOL_FACE_EAST;
                best.t = t;
                best.pos = glm::vec3(halfExtent.x, p.y, p.z);
                best.normal = glm::vec3(-1.0f, 0.0f, 0.0f);
            }
        }
    }

    // North wall: y = +halfExtent.y, inward normal (0, -1, 0)
    if (dir.y > 1e-6f) {
        const float t = (halfExtent.y - origin.y) / dir.y;
        if (t > 0.0f && t < bestT) {
            const glm::vec3 p = origin + dir * t;
            if (p.x >= -halfExtent.x - eps && p.x <= halfExtent.x + eps &&
                p.z >= bottomZ - eps && p.z <= lipZ + eps) {
                bestT = t;
                best.hit = true;
                best.face = POOL_FACE_NORTH;
                best.t = t;
                best.pos = glm::vec3(p.x, halfExtent.y, p.z);
                best.normal = glm::vec3(0.0f, -1.0f, 0.0f);
            }
        }
    }

    // West wall: x = -halfExtent.x, inward normal (+1, 0, 0)
    if (dir.x < -1e-6f) {
        const float t = (-halfExtent.x - origin.x) / dir.x;
        if (t > 0.0f && t < bestT) {
            const glm::vec3 p = origin + dir * t;
            if (p.y >= -halfExtent.y - eps && p.y <= halfExtent.y + eps &&
                p.z >= bottomZ - eps && p.z <= lipZ + eps) {
                bestT = t;
                best.hit = true;
                best.face = POOL_FACE_WEST;
                best.t = t;
                best.pos = glm::vec3(-halfExtent.x, p.y, p.z);
                best.normal = glm::vec3(1.0f, 0.0f, 0.0f);
            }
        }
    }

    return best;
}

inline glm::vec2 unfoldPoolPoint(const glm::vec3& pos,
                                 int face,
                                 const glm::vec2& halfExtent,
                                 float bottomZ) {
    const float dz = std::max(0.0f, pos.z - bottomZ);
    switch (face) {
        case POOL_FACE_FLOOR:
            return glm::vec2(pos.x, pos.y);
        case POOL_FACE_SOUTH:
            return glm::vec2(pos.x, -halfExtent.y - dz);
        case POOL_FACE_EAST:
            return glm::vec2(halfExtent.x + dz, pos.y);
        case POOL_FACE_NORTH:
            return glm::vec2(pos.x, halfExtent.y + dz);
        case POOL_FACE_WEST:
            return glm::vec2(-halfExtent.x - dz, pos.y);
        default:
            return glm::vec2(pos.x, pos.y);
    }
}

inline glm::vec2 poolUnfoldedToUV(const glm::vec2& unfolded,
                                  const glm::vec2& halfExtent,
                                  float bottomZ,
                                  float lipZ) {
    const glm::vec2 halfSpan = poolAtlasHalfSpan(halfExtent, bottomZ, lipZ);
    return (unfolded + halfSpan) / (2.0f * halfSpan);
}

inline glm::vec2 poolPointToAtlasUV(const glm::vec3& pos,
                                    int face,
                                    const glm::vec2& halfExtent,
                                    float bottomZ,
                                    float lipZ) {
    return poolUnfoldedToUV(unfoldPoolPoint(pos, face, halfExtent, bottomZ), halfExtent, bottomZ, lipZ);
}

inline float rimVisibility(const glm::vec3& pos,
                           const glm::vec3& lightPos,
                           const glm::vec2& halfExtent,
                           float lipZ,
                           float penumbra = 1.5f) {
    const glm::vec3 d = lightPos - pos;
    float tExit = 1e30f;

    if (d.x > 1e-6f) {
        const float t = (halfExtent.x - pos.x) / d.x;
        if (t > 1e-5f) tExit = std::min(tExit, t);
    } else if (d.x < -1e-6f) {
        const float t = (-halfExtent.x - pos.x) / d.x;
        if (t > 1e-5f) tExit = std::min(tExit, t);
    }

    if (d.y > 1e-6f) {
        const float t = (halfExtent.y - pos.y) / d.y;
        if (t > 1e-5f) tExit = std::min(tExit, t);
    } else if (d.y < -1e-6f) {
        const float t = (-halfExtent.y - pos.y) / d.y;
        if (t > 1e-5f) tExit = std::min(tExit, t);
    }

    if (tExit >= 1.0f) {
        return 1.0f;
    }

    const float zExit = pos.z + tExit * d.z;
    const float halfP = std::max(1e-4f, 0.5f * penumbra);
    const float u = std::clamp((zExit - (lipZ - halfP)) / (2.0f * halfP), 0.0f, 1.0f);
    return u * u * (3.0f - 2.0f * u);
}

#endif

namespace {

int g_failures = 0;

void fail(const std::string& msg) {
    std::cerr << "FAIL " << msg << "\n";
    ++g_failures;
}

bool near(float a, float b, float tol = 1e-4f) {
    return std::fabs(a - b) <= tol;
}

bool near2(const glm::vec2& a, const glm::vec2& b, float tol = 1e-4f) {
    return near(a.x, b.x, tol) && near(a.y, b.y, tol);
}

bool near3(const glm::vec3& a, const glm::vec3& b, float tol = 1e-3f) {
    return near(a.x, b.x, tol) && near(a.y, b.y, tol) && near(a.z, b.z, tol);
}

void check_pool_bijection_case(const char* label, int w, int h, float scale) {
    PoolConfig pool;
    pool.gridW = w;
    pool.gridH = h;
    pool.waterScale = scale;

    const float hw = pool.halfWidth();
    const float hh = pool.halfHeight();

    const glm::vec2 p00 = pool.gridToWorld(0.0f, 0.0f);
    const glm::vec2 pMax = pool.gridToWorld(static_cast<float>(w - 1), static_cast<float>(h - 1));
    if (!near2(p00, glm::vec2(-hw, -hh), 1e-4f)) {
        fail(std::string(label) + ": gridToWorld(0,0) != (-hw,-hh)");
    }
    if (!near2(pMax, glm::vec2(hw, hh), 1e-4f)) {
        fail(std::string(label) + ": gridToWorld(W-1,H-1) != (+hw,+hh) (east/north gap regression)");
    }

    const float sampleFracs[] = {0.0f, 0.1f, 0.25f, 0.5f, 0.73f, 0.99f, 1.0f};
    float maxRoundtripErr = 0.0f;
    for (float fx : sampleFracs) {
        for (float fy : sampleFracs) {
            const float gx = fx * static_cast<float>(w - 1);
            const float gy = fy * static_cast<float>(h - 1);
            const glm::vec2 world = pool.gridToWorld(gx, gy);
            const glm::vec2 back = pool.worldToGrid(world.x, world.y);
            const float err = std::max(std::fabs(back.x - gx), std::fabs(back.y - gy));
            maxRoundtripErr = std::max(maxRoundtripErr, err);
        }
    }

    const glm::vec2 uvMin = pool.worldToUV(-hw, -hh);
    const glm::vec2 uvMax = pool.worldToUV(hw, hh);
    if (!near2(uvMin, glm::vec2(0.0f, 0.0f), 1e-5f) || !near2(uvMax, glm::vec2(1.0f, 1.0f), 1e-5f)) {
        fail(std::string(label) + ": worldToUV boundary mapping mismatch");
    }
    if (maxRoundtripErr > 1e-3f) {
        fail(std::string(label) + ": worldToGrid(gridToWorld) round-trip error " +
             std::to_string(maxRoundtripErr));
    }

    std::cout << "pool_coords [" << label << " " << w << "x" << h << "]: "
              << "extent=(" << hw << "," << hh << ") max_roundtrip_err=" << maxRoundtripErr << "\n";
}

void test_pool_config_bijection() {
    check_pool_bijection_case("square_400x400", 400, 400, 1.0f);
    check_pool_bijection_case("rect_300x500", 300, 500, 1.0f);
    check_pool_bijection_case("rect_scaled", 240, 360, 0.5f);
}

void test_pool_box_hit() {
    const glm::vec2 halfExt(200.0f, 200.0f);
    const float bottomZ = -30.0f;
    const float lipZ = 18.0f;

    // Floor hit
    {
        const PoolBoxHit h = poolBoxHit(glm::vec3(12.0f, -25.0f, 0.0f),
                                        glm::vec3(0.0f, 0.0f, -1.0f),
                                        halfExt, bottomZ, lipZ);
        if (!h.hit || h.face != POOL_FACE_FLOOR || !near3(h.pos, glm::vec3(12.0f, -25.0f, bottomZ)) ||
            !near(h.t, 30.0f, 1e-3f)) {
            fail("poolBoxHit: downward ray did not hit floor accurately");
        }
    }

    // South wall hit (y = -200)
    {
        const glm::vec3 dir = glm::normalize(glm::vec3(0.0f, -1.0f, -0.5f));
        const PoolBoxHit h = poolBoxHit(glm::vec3(15.0f, -185.0f, 0.0f), dir, halfExt, bottomZ, lipZ);
        if (!h.hit || h.face != POOL_FACE_SOUTH || !near(h.pos.y, -200.0f, 1e-3f) ||
            !(h.pos.z > bottomZ && h.pos.z < 0.0f)) {
            fail("poolBoxHit: outward south ray did not hit South wall before floor");
        }
    }

    // East wall hit (x = +200)
    {
        const glm::vec3 dir = glm::normalize(glm::vec3(1.0f, 0.0f, -0.5f));
        const PoolBoxHit h = poolBoxHit(glm::vec3(185.0f, 25.0f, 0.0f), dir, halfExt, bottomZ, lipZ);
        if (!h.hit || h.face != POOL_FACE_EAST || !near(h.pos.x, 200.0f, 1e-3f) ||
            !(h.pos.z > bottomZ && h.pos.z < 0.0f)) {
            fail("poolBoxHit: outward east ray did not hit East wall before floor");
        }
    }

    // North wall hit (y = +200)
    {
        const glm::vec3 dir = glm::normalize(glm::vec3(0.0f, 1.0f, -0.5f));
        const PoolBoxHit h = poolBoxHit(glm::vec3(-30.0f, 185.0f, 0.0f), dir, halfExt, bottomZ, lipZ);
        if (!h.hit || h.face != POOL_FACE_NORTH || !near(h.pos.y, 200.0f, 1e-3f) ||
            !(h.pos.z > bottomZ && h.pos.z < 0.0f)) {
            fail("poolBoxHit: outward north ray did not hit North wall before floor");
        }
    }

    // West wall hit (x = -200)
    {
        const glm::vec3 dir = glm::normalize(glm::vec3(-1.0f, 0.0f, -0.5f));
        const PoolBoxHit h = poolBoxHit(glm::vec3(-185.0f, -40.0f, 0.0f), dir, halfExt, bottomZ, lipZ);
        if (!h.hit || h.face != POOL_FACE_WEST || !near(h.pos.x, -200.0f, 1e-3f) ||
            !(h.pos.z > bottomZ && h.pos.z < 0.0f)) {
            fail("poolBoxHit: outward west ray did not hit West wall before floor");
        }
    }

    // Rectangular pool check (halfExtent = 150 x 250)
    {
        const glm::vec2 rectHalf(150.0f, 250.0f);
        const PoolBoxHit he = poolBoxHit(glm::vec3(140.0f, 0.0f, 0.0f),
                                         glm::normalize(glm::vec3(1.0f, 0.0f, -0.5f)),
                                         rectHalf, bottomZ, lipZ);
        const PoolBoxHit hn = poolBoxHit(glm::vec3(0.0f, 240.0f, 0.0f),
                                         glm::normalize(glm::vec3(0.0f, 1.0f, -0.5f)),
                                         rectHalf, bottomZ, lipZ);
        if (!he.hit || he.face != POOL_FACE_EAST || !near(he.pos.x, 150.0f, 1e-3f) ||
            !hn.hit || hn.face != POOL_FACE_NORTH || !near(hn.pos.y, 250.0f, 1e-3f)) {
            fail("poolBoxHit: rectangular pool wall hits failed");
        }
    }

    std::cout << "pool_box_hit: floor + 4 walls (square & rectangular) verified\n";
}

void check_crease_continuity(const char* label, const glm::vec2& halfExt, float bottomZ, float lipZ) {
    const glm::vec2 span = poolAtlasSpan(halfExt, bottomZ, lipZ);
    const float eps = 0.05f;
    const float sampleCoord = 37.5f;

    struct CreaseCase {
        const char* name;
        int wallFace;
        glm::vec3 seamPoint;
        glm::vec3 floorNear;
        glm::vec3 wallNear;
        float spanAxis;
    };

    const CreaseCase cases[] = {
        {
            "south", POOL_FACE_SOUTH,
            glm::vec3(sampleCoord, -halfExt.y, bottomZ),
            glm::vec3(sampleCoord, -halfExt.y + eps, bottomZ),
            glm::vec3(sampleCoord, -halfExt.y, bottomZ + eps),
            span.y
        },
        {
            "east", POOL_FACE_EAST,
            glm::vec3(halfExt.x, sampleCoord, bottomZ),
            glm::vec3(halfExt.x - eps, sampleCoord, bottomZ),
            glm::vec3(halfExt.x, sampleCoord, bottomZ + eps),
            span.x
        },
        {
            "north", POOL_FACE_NORTH,
            glm::vec3(sampleCoord, halfExt.y, bottomZ),
            glm::vec3(sampleCoord, halfExt.y - eps, bottomZ),
            glm::vec3(sampleCoord, halfExt.y, bottomZ + eps),
            span.y
        },
        {
            "west", POOL_FACE_WEST,
            glm::vec3(-halfExt.x, sampleCoord, bottomZ),
            glm::vec3(-halfExt.x + eps, sampleCoord, bottomZ),
            glm::vec3(-halfExt.x, sampleCoord, bottomZ + eps),
            span.x
        },
    };

    float maxRatio = 0.0f;
    for (const auto& c : cases) {
        const glm::vec2 uvSeamFloor = poolPointToAtlasUV(c.seamPoint, POOL_FACE_FLOOR, halfExt, bottomZ, lipZ);
        const glm::vec2 uvSeamWall = poolPointToAtlasUV(c.seamPoint, c.wallFace, halfExt, bottomZ, lipZ);
        const float seamGap = glm::length(uvSeamFloor - uvSeamWall);
        if (seamGap > 1e-5f) {
            fail(std::string(label) + " " + c.name + ": exact floor-wall crease UV jump = " +
                 std::to_string(seamGap));
        }

        const glm::vec2 uvFloor = poolPointToAtlasUV(c.floorNear, POOL_FACE_FLOOR, halfExt, bottomZ, lipZ);
        const glm::vec2 uvWall = poolPointToAtlasUV(c.wallNear, c.wallFace, halfExt, bottomZ, lipZ);
        const float dist = glm::length(uvFloor - uvWall);
        const float expected = (2.0f * eps) / c.spanAxis;
        maxRatio = std::max(maxRatio, dist / expected);

        if (dist > expected * 1.02f) {
            fail(std::string(label) + " " + c.name + ": UV distance across crease " +
                 std::to_string(dist) + " exceeded 2*eps/span (" + std::to_string(expected) + ")");
        }
        if (uvFloor.x < 0.0f || uvFloor.x > 1.0f || uvFloor.y < 0.0f || uvFloor.y > 1.0f ||
            uvWall.x < 0.0f || uvWall.x > 1.0f || uvWall.y < 0.0f || uvWall.y > 1.0f) {
            fail(std::string(label) + " " + c.name + ": unfolded atlas UV fell outside [0,1]^2");
        }
    }

    std::cout << "atlas_continuity [" << label << "]: 4 floor-wall creases continuous (max dist/(2*eps/span)="
              << maxRatio << ")\n";
}

void test_unfolded_atlas_continuity() {
    check_crease_continuity("square_400x400", glm::vec2(200.0f, 200.0f), -30.0f, 18.0f);
    check_crease_continuity("rect_300x500", glm::vec2(150.0f, 250.0f), -30.0f, 18.0f);
}

void test_rim_occlusion() {
    const glm::vec2 halfExt(200.0f, 200.0f);
    const float lipZ = 18.0f;

    // Overhead light should leave pool interior unshadowed
    const glm::vec3 overheadLight(0.0f, 0.0f, 100.0f);
    const float visCenter = rimVisibility(glm::vec3(0.0f, 0.0f, -30.0f), overheadLight, halfExt, lipZ);
    const float visNearWall = rimVisibility(glm::vec3(-190.0f, 0.0f, -30.0f), overheadLight, halfExt, lipZ);
    const float visSubWall = rimVisibility(glm::vec3(-199.5f, 0.0f, -10.0f), overheadLight, halfExt, lipZ);

    if (visCenter < 0.99f || visNearWall < 0.99f || visSubWall < 0.99f) {
        fail("rimVisibility: overhead light should be unshadowed (got " +
             std::to_string(visCenter) + ", " + std::to_string(visNearWall) + ", " +
             std::to_string(visSubWall) + ")");
    }

    // Low-angle light blocked by East rim wall (z_exit << lipZ) must be shadowed
    const glm::vec3 lowLight(400.0f, 0.0f, 25.0f);
    const float visBlocked = rimVisibility(glm::vec3(190.0f, 0.0f, -25.0f), lowLight, halfExt, lipZ);
    if (visBlocked > 0.01f) {
        fail("rimVisibility: low-angle ray blocked by rim should be shadowed (got " +
             std::to_string(visBlocked) + ")");
    }

    // High-enough point on opposite side of pool clears the East rim (z_exit > lipZ) -> unshadowed
    const float visClearsRim = rimVisibility(glm::vec3(-190.0f, 0.0f, 15.0f),
                                             glm::vec3(400.0f, 0.0f, 120.0f),
                                             halfExt, lipZ);
    if (visClearsRim < 0.99f) {
        fail("rimVisibility: ray clearing rim above lipZ should be unshadowed (got " +
             std::to_string(visClearsRim) + ")");
    }

    std::cout << "rim_occlusion: overhead=" << visCenter
              << " blocked_low=" << visBlocked
              << " clears_rim=" << visClearsRim << "\n";
}

} // namespace

int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    std::cout << "optics_tests source: "
              << (HAS_REAL_POOL_OPTICS ? "src/render/pool_optics.h" : "standalone Track 0 reference fallback")
              << "\n";

    test_pool_config_bijection();
    test_pool_box_hit();
    test_unfolded_atlas_continuity();
    test_rim_occlusion();

    if (g_failures != 0) {
        std::cerr << "optics tests: FAIL (" << g_failures << " failure(s))\n";
        return 1;
    }
    std::cout << "optics tests: PASS\n";
    return 0;
}
