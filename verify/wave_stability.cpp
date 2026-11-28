#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "sim/wave_sim.h"

static const int kCornerSteps = 200;
static const float kCornerPeakCeiling = 4.0f;
static const float kCornerFinalCeiling = 2.0f;

static const int kPoolSteps = 500;
static const float kCheckerPeakCeiling = 14.0f;
static const float kSettledHeight = 1.0f;

static int failures = 0;

static void fail(const std::string& message) {
    std::cerr << "FAIL " << message << "\n";
    ++failures;
}

static void zero_grids(WaveSim& sim) {
    const int cells = sim.width * sim.height;
    for (int i = 0; i < cells; ++i) {
        sim.height_current[i] = 0.0f;
        sim.height_prev[i] = 0.0f;
        sim.height_next[i] = 0.0f;
    }
}

static bool finite_grids(const WaveSim& sim) {
    const int cells = sim.width * sim.height;
    for (int i = 0; i < cells; ++i) {
        if (!std::isfinite(sim.height_current[i]) || !std::isfinite(sim.height_prev[i])) {
            return false;
        }
    }
    return true;
}

struct Cell {
    float value;
    int x;
    int y;
};

static Cell tallest(const WaveSim& sim) {
    Cell best{0.0f, 0, 0};
    for (int y = 0; y < sim.height; ++y) {
        for (int x = 0; x < sim.width; ++x) {
            const float raw = sim.height_current[sim.idx(x, y)];
            const float mag = std::fabs(raw);
            if (mag > std::fabs(best.value)) {
                best.value = raw;
                best.x = x;
                best.y = y;
            }
        }
    }
    return best;
}

static void load_corner(WaveSim& sim, std::uint32_t mask) {
    zero_grids(sim);
    for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
            const int bit = x * 4 + y;
            sim.height_current[sim.idx(x, y)] = (mask & (1u << bit)) ? 1.0f : -1.0f;
        }
    }
    const int interior[4][2] = {{1, 1}, {1, 2}, {2, 1}, {2, 2}};
    for (int k = 0; k < 4; ++k) {
        sim.height_prev[sim.idx(interior[k][0], interior[k][1])] =
            (mask & (1u << (16 + k))) ? 1.0f : -1.0f;
    }
}

static bool close_enough(float a, float b) {
    const float scale = std::max(1.0f, std::max(std::fabs(a), std::fabs(b)));
    return std::fabs(a - b) <= 1e-4f * scale;
}

static void check_linear() {
    WaveSim sim;
    sim.init(4, 4);

    load_corner(sim, 0x000abcdeu);
    const std::vector<float> a_cur = sim.height_current;
    const std::vector<float> a_prev = sim.height_prev;
    sim.step();
    const std::vector<float> out_a = sim.height_current;

    load_corner(sim, 0x00054321u);
    const std::vector<float> b_cur = sim.height_current;
    const std::vector<float> b_prev = sim.height_prev;
    sim.step();
    const std::vector<float> out_b = sim.height_current;

    zero_grids(sim);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            const int i = sim.idx(x, y);
            sim.height_current[i] = a_cur[i] + b_cur[i];
            sim.height_prev[i] = a_prev[i] + b_prev[i];
        }
    }
    sim.step();
    const std::vector<float> out_sum = sim.height_current;

    bool linear = true;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            const int i = sim.idx(x, y);
            if (!close_enough(out_sum[i], out_a[i] + out_b[i])) {
                linear = false;
            }
        }
    }
    if (!linear) {
        fail("WaveSim::step is not linear, so the 4x4 corner sweep does not cover every surface in [-1, 1]");
    }

    zero_grids(sim);
    sim.step();
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            if (sim.height_current[sim.idx(x, y)] != 0.0f) {
                fail("WaveSim::step moved a flat zero surface at cell (" + std::to_string(x) + "," +
                     std::to_string(y) + ")");
                return;
            }
        }
    }
}

static void check_row_major_layout() {
    WaveSim rect;
    rect.init(10, 6);
    rect.addDisturbance(7, 3, 2.5f);
    if (rect.idx(7, 3) != 3 * 10 + 7 || rect.height_current[3 * 10 + 7] != 2.5f) {
        fail("WaveSim storage is not canonical row-major (y * width + x)");
    }
}

static void sweep_corners() {
    WaveSim sim;
    sim.init(4, 4);
    float worst_peak = 0.0f;
    float worst_final = 0.0f;
    std::uint32_t peak_mask = 0;
    int peak_step = 0;
    const std::uint32_t corners = 1u << 20;

    for (std::uint32_t mask = 0; mask < corners; ++mask) {
        load_corner(sim, mask);
        for (int step = 1; step <= kCornerSteps; ++step) {
            sim.step();
            if (!finite_grids(sim)) {
                const Cell cell = tallest(sim);
                fail("4x4 corner mask " + std::to_string(mask) + " produced a non-finite height at step " +
                     std::to_string(step) + " cell (" + std::to_string(cell.x) + "," + std::to_string(cell.y) + ")");
                return;
            }
            const Cell cell = tallest(sim);
            const float mag = std::fabs(cell.value);
            if (mag > kCornerPeakCeiling) {
                fail("4x4 corner mask " + std::to_string(mask) + ": after step " +
                     std::to_string(step) + ", cell (" + std::to_string(cell.x) + "," + std::to_string(cell.y) +
                     ") height " + std::to_string(cell.value) + " is above the ceiling " +
                     std::to_string(kCornerPeakCeiling));
                return;
            }
            if (mag > worst_peak) {
                worst_peak = mag;
                peak_mask = mask;
                peak_step = step;
            }
        }
        worst_final = std::max(worst_final, std::fabs(tallest(sim).value));
    }

    if (worst_final > kCornerFinalCeiling) {
        fail("4x4 surfaces were still taller than " + std::to_string(kCornerFinalCeiling) + " after " +
             std::to_string(kCornerSteps) + " steps (tallest " + std::to_string(worst_final) + ")");
        return;
    }

    load_corner(sim, peak_mask);
    Cell witness{0.0f, 0, 0};
    for (int step = 1; step <= peak_step; ++step) {
        sim.step();
        witness = tallest(sim);
    }
    std::cout << "4x4 every surface in [-1, 1]: tallest height " << std::fabs(witness.value)
              << " at step " << peak_step << " cell (" << witness.x << "," << witness.y
              << ") corner mask " << peak_mask << ". At step " << kCornerSteps << " the tallest was "
              << worst_final << ".\n";
}

static void run_pool(const char* name, void (*setup)(WaveSim&), float peak_ceiling) {
    WaveSim sim;
    sim.init(400, 400);
    setup(sim);
    float peak = std::fabs(tallest(sim).value);
    int peak_step = 0;
    Cell peak_cell = tallest(sim);
    for (int step = 1; step <= kPoolSteps; ++step) {
        sim.step();
        if (!finite_grids(sim)) {
            const Cell cell = tallest(sim);
            fail(std::string(name) + ": non-finite height after step " + std::to_string(step) +
                 " cell (" + std::to_string(cell.x) + "," + std::to_string(cell.y) + ")");
            return;
        }
        const Cell cell = tallest(sim);
        const float mag = std::fabs(cell.value);
        if (mag > peak_ceiling) {
            fail(std::string(name) + ": after step " + std::to_string(step) + ", cell (" +
                 std::to_string(cell.x) + "," + std::to_string(cell.y) + ") height " + std::to_string(cell.value) +
                 " is above the ceiling " + std::to_string(peak_ceiling));
            return;
        }
        if (mag > peak) {
            peak = mag;
            peak_step = step;
            peak_cell = cell;
        }
    }
    const Cell final_cell = tallest(sim);
    if (std::fabs(final_cell.value) >= kSettledHeight) {
        fail(std::string(name) + ": after step " + std::to_string(kPoolSteps) + ", cell (" +
             std::to_string(final_cell.x) + "," + std::to_string(final_cell.y) + ") height " +
             std::to_string(final_cell.value) + " has not fallen below " + std::to_string(kSettledHeight));
        return;
    }
    std::cout << name << ": peak height " << peak << " at step " << peak_step << " cell (" << peak_cell.x << ","
              << peak_cell.y << "). At step " << kPoolSteps << " the tallest is " << final_cell.value << ".\n";
}

static void startup_splashes(WaveSim& sim) {
    sim.init(400, 400);
    sim.addDisturbance(100, 100, 2.0f);
    sim.addDisturbance(300, 300, 1.5f);
    sim.addDisturbance(150, 250, 1.8f);
}

static void mouse_click(WaveSim& sim) {
    sim.init(400, 400);
    sim.addDisturbance(200, 200, 5.0f);
}

static void checkerboard(WaveSim& sim) {
    for (int y = 1; y < sim.height - 1; ++y) {
        for (int x = 1; x < sim.width - 1; ++x) {
            const float sign = ((x + y) % 2 == 0) ? 1.0f : -1.0f;
            sim.height_current[sim.idx(x, y)] = sign;
            sim.height_prev[sim.idx(x, y)] = sign;
        }
    }
}

int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    WaveSim defaults;
    if (defaults.width != 400 || defaults.height != 400) {
        fail("WaveSim default grid is " + std::to_string(defaults.width) + "x" + std::to_string(defaults.height) +
             ", not 400x400. The pool scenarios and ceilings no longer match the code.");
        return 1;
    }

    std::cout << "parameters dx " << defaults.dx << " dt " << defaults.dt
              << " c " << defaults.c << " damping " << defaults.damping << "\n";

    check_row_major_layout();
    check_linear();
    if (failures != 0) {
        std::cerr << "wave stability: FAIL\n";
        return 1;
    }

    run_pool("checkerboard", checkerboard, kCheckerPeakCeiling);
    run_pool("startup splashes", startup_splashes, 2.0f);
    run_pool("click of 5", mouse_click, 5.0f);
    if (failures != 0) {
        std::cerr << "wave stability: FAIL\n";
        return 1;
    }

    sweep_corners();
    if (failures != 0) {
        std::cerr << "wave stability: FAIL\n";
        return 1;
    }

    std::cout << "wave stability: PASS\n";
    return 0;
}
