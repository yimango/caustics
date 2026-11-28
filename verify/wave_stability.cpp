#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "sim/wave_sim.h"

static const int kCornerSteps = 200;
static const float kDirichletCornerPeakCeiling = 5.0f;
static const float kDirichletCornerFinalCeiling = 2.0f;
static const float kNeumannCornerDevPeakCeiling = 8.0f;
static const float kNeumannCornerDevFinalCeiling = 2.5f;
static const float kNeumannCornerAbsCeiling = 500.0f;

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

static double weighted_mean(const WaveSim& sim) {
    double wsum = 0.0;
    double vsum = 0.0;
    for (int y = 0; y < sim.height; ++y) {
        const double wy = (y == 0 || y == sim.height - 1) ? 0.5 : 1.0;
        for (int x = 0; x < sim.width; ++x) {
            const double wx = (x == 0 || x == sim.width - 1) ? 0.5 : 1.0;
            const double w = wx * wy;
            wsum += w;
            vsum += w * static_cast<double>(sim.height_current[sim.idx(x, y)]);
        }
    }
    return (wsum > 0.0) ? (vsum / wsum) : 0.0;
}

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

static Cell tallest_deviation(const WaveSim& sim, float mean) {
    Cell best{0.0f, 0, 0};
    for (int y = 0; y < sim.height; ++y) {
        for (int x = 0; x < sim.width; ++x) {
            const float dev = sim.height_current[sim.idx(x, y)] - mean;
            const float mag = std::fabs(dev);
            if (mag > std::fabs(best.value)) {
                best.value = dev;
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

static void check_linear(BoundaryMode mode, const char* label) {
    WaveSim sim;
    sim.boundaryMode = mode;
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
        fail(std::string(label) + ": WaveSim::step is not linear, so the 4x4 corner sweep does not cover every surface in [-1, 1]");
    }

    zero_grids(sim);
    sim.step();
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            if (sim.height_current[sim.idx(x, y)] != 0.0f) {
                fail(std::string(label) + ": WaveSim::step moved a flat zero surface at cell (" +
                     std::to_string(x) + "," + std::to_string(y) + ")");
                return;
            }
        }
    }
}

static void check_row_major_layout() {
    WaveSim rect;
    rect.init(10, 6);
    rect.addDisturbance(7, 3, 2.5f, 0.0f);
    if (rect.idx(7, 3) != 3 * 10 + 7 || std::fabs(rect.height_current[3 * 10 + 7] - 2.5f) > 1e-5f) {
        fail("WaveSim storage is not canonical row-major (y * width + x)");
    }
}

static void sweep_corners_dirichlet() {
    WaveSim sim;
    sim.boundaryMode = BoundaryMode::FixedDirichlet;
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
                fail("4x4 FixedDirichlet corner mask " + std::to_string(mask) +
                     " produced a non-finite height at step " + std::to_string(step) +
                     " cell (" + std::to_string(cell.x) + "," + std::to_string(cell.y) + ")");
                return;
            }
            const Cell cell = tallest(sim);
            const float mag = std::fabs(cell.value);
            if (mag > kDirichletCornerPeakCeiling) {
                fail("4x4 FixedDirichlet corner mask " + std::to_string(mask) + ": after step " +
                     std::to_string(step) + ", cell (" + std::to_string(cell.x) + "," + std::to_string(cell.y) +
                     ") height " + std::to_string(cell.value) + " is above the ceiling " +
                     std::to_string(kDirichletCornerPeakCeiling));
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

    if (worst_final > kDirichletCornerFinalCeiling) {
        fail("4x4 FixedDirichlet surfaces were still taller than " +
             std::to_string(kDirichletCornerFinalCeiling) + " after " +
             std::to_string(kCornerSteps) + " steps (tallest " + std::to_string(worst_final) + ")");
        return;
    }

    load_corner(sim, peak_mask);
    Cell witness{0.0f, 0, 0};
    for (int step = 1; step <= peak_step; ++step) {
        sim.step();
        witness = tallest(sim);
    }
    std::cout << "4x4 FixedDirichlet every surface in [-1, 1]: tallest height " << std::fabs(witness.value)
              << " at step " << peak_step << " cell (" << witness.x << "," << witness.y
              << ") corner mask " << peak_mask << ". At step " << kCornerSteps << " the tallest was "
              << worst_final << ".\n";
}

static void sweep_corners_neumann() {
    WaveSim sim;
    sim.boundaryMode = BoundaryMode::ReflectiveNeumann;
    sim.init(4, 4);
    float worst_dev_peak = 0.0f;
    float worst_dev_final = 0.0f;
    float worst_abs_peak = 0.0f;
    std::uint32_t peak_mask = 0;
    int peak_step = 0;
    const std::uint32_t corners = 1u << 20;

    for (std::uint32_t mask = 0; mask < corners; ++mask) {
        load_corner(sim, mask);
        for (int step = 1; step <= kCornerSteps; ++step) {
            sim.step();
            if (!finite_grids(sim)) {
                const Cell cell = tallest(sim);
                fail("4x4 ReflectiveNeumann corner mask " + std::to_string(mask) +
                     " produced a non-finite height at step " + std::to_string(step) +
                     " cell (" + std::to_string(cell.x) + "," + std::to_string(cell.y) + ")");
                return;
            }
            const float mean = static_cast<float>(weighted_mean(sim));
            const Cell abs_cell = tallest(sim);
            const float abs_mag = std::fabs(abs_cell.value);
            if (abs_mag > kNeumannCornerAbsCeiling) {
                fail("4x4 ReflectiveNeumann corner mask " + std::to_string(mask) + ": after step " +
                     std::to_string(step) + ", cell (" + std::to_string(abs_cell.x) + "," +
                     std::to_string(abs_cell.y) + ") absolute height " + std::to_string(abs_cell.value) +
                     " exceeded bound " + std::to_string(kNeumannCornerAbsCeiling));
                return;
            }
            if (abs_mag > worst_abs_peak) {
                worst_abs_peak = abs_mag;
            }
            const Cell dev_cell = tallest_deviation(sim, mean);
            const float dev_mag = std::fabs(dev_cell.value);
            if (dev_mag > kNeumannCornerDevPeakCeiling) {
                fail("4x4 ReflectiveNeumann corner mask " + std::to_string(mask) + ": after step " +
                     std::to_string(step) + ", cell (" + std::to_string(dev_cell.x) + "," +
                     std::to_string(dev_cell.y) + ") deviation " + std::to_string(dev_cell.value) +
                     " is above the ceiling " + std::to_string(kNeumannCornerDevPeakCeiling));
                return;
            }
            if (dev_mag > worst_dev_peak) {
                worst_dev_peak = dev_mag;
                peak_mask = mask;
                peak_step = step;
            }
        }
        const float final_mean = static_cast<float>(weighted_mean(sim));
        worst_dev_final = std::max(worst_dev_final, std::fabs(tallest_deviation(sim, final_mean).value));
    }

    if (worst_dev_final > kNeumannCornerDevFinalCeiling) {
        fail("4x4 ReflectiveNeumann deviation from mean was still above " +
             std::to_string(kNeumannCornerDevFinalCeiling) + " after " +
             std::to_string(kCornerSteps) + " steps (tallest " + std::to_string(worst_dev_final) + ")");
        return;
    }

    std::cout << "4x4 ReflectiveNeumann every surface in [-1, 1]: peak deviation " << worst_dev_peak
              << " at step " << peak_step << " corner mask " << peak_mask
              << ", final deviation " << worst_dev_final
              << ", max abs height " << worst_abs_peak << ".\n";
}

static void run_pool(const char* name, BoundaryMode mode, const char* mode_label,
                     void (*setup)(WaveSim&), float peak_ceiling) {
    WaveSim sim;
    sim.boundaryMode = mode;
    sim.init(400, 400);
    setup(sim);

    const bool subtract_mean = (mode == BoundaryMode::ReflectiveNeumann);
    const float init_mean = subtract_mean ? static_cast<float>(weighted_mean(sim)) : 0.0f;
    Cell peak_cell = subtract_mean ? tallest_deviation(sim, init_mean) : tallest(sim);
    float peak = std::fabs(peak_cell.value);
    float max_abs = std::fabs(tallest(sim).value);
    int peak_step = 0;

    for (int step = 1; step <= kPoolSteps; ++step) {
        sim.step();
        if (!finite_grids(sim)) {
            const Cell cell = tallest(sim);
            fail(std::string(name) + " [" + mode_label + "]: non-finite height after step " +
                 std::to_string(step) + " cell (" + std::to_string(cell.x) + "," + std::to_string(cell.y) + ")");
            return;
        }
        const float mean = subtract_mean ? static_cast<float>(weighted_mean(sim)) : 0.0f;
        const Cell abs_cell = tallest(sim);
        const float abs_mag = std::fabs(abs_cell.value);
        max_abs = std::max(max_abs, abs_mag);
        const Cell cell = subtract_mean ? tallest_deviation(sim, mean) : abs_cell;
        const float mag = std::fabs(cell.value);
        if (mag > peak_ceiling) {
            fail(std::string(name) + " [" + mode_label + "]: after step " + std::to_string(step) +
                 ", cell (" + std::to_string(cell.x) + "," + std::to_string(cell.y) + ") " +
                 (subtract_mean ? "deviation " : "height ") + std::to_string(cell.value) +
                 " is above the ceiling " + std::to_string(peak_ceiling));
            return;
        }
        if (subtract_mean && abs_mag > peak_ceiling + 10.0f) {
            fail(std::string(name) + " [" + mode_label + "]: after step " + std::to_string(step) +
                 ", absolute height " + std::to_string(abs_mag) + " exceeded strict bound " +
                 std::to_string(peak_ceiling + 10.0f));
            return;
        }
        if (mag > peak) {
            peak = mag;
            peak_step = step;
            peak_cell = cell;
        }
    }

    const float final_mean = subtract_mean ? static_cast<float>(weighted_mean(sim)) : 0.0f;
    const Cell final_cell = subtract_mean ? tallest_deviation(sim, final_mean) : tallest(sim);
    if (std::fabs(final_cell.value) >= kSettledHeight) {
        fail(std::string(name) + " [" + mode_label + "]: after step " + std::to_string(kPoolSteps) +
             ", cell (" + std::to_string(final_cell.x) + "," + std::to_string(final_cell.y) + ") " +
             (subtract_mean ? "deviation " : "height ") + std::to_string(final_cell.value) +
             " has not fallen below " + std::to_string(kSettledHeight));
        return;
    }
    std::cout << name << " [" << mode_label << "]: peak " << (subtract_mean ? "dev " : "height ")
              << peak << " at step " << peak_step << " cell (" << peak_cell.x << "," << peak_cell.y
              << "). At step " << kPoolSteps << " settled to " << final_cell.value
              << " (mean " << final_mean << ", max_abs " << max_abs << ").\n";
}

static void startup_splashes(WaveSim& sim) {
    sim.addDisturbance(100, 100, 2.0f, 0.0f);
    sim.addDisturbance(300, 300, 1.5f, 0.0f);
    sim.addDisturbance(150, 250, 1.8f, 0.0f);
}

static void mouse_click(WaveSim& sim) {
    sim.addDisturbance(200, 200, 5.0f, 0.0f);
}

static void smooth_splash(WaveSim& sim) {
    sim.addDisturbance(200, 200, 5.0f, 4.0f);
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
        fail("WaveSim default grid is " + std::to_string(defaults.width) + "x" +
             std::to_string(defaults.height) + ", not 400x400.");
        return 1;
    }
    const float courant = (defaults.dx > 0.0f) ? (defaults.c * defaults.dt / defaults.dx) : 999.0f;
    if (!(defaults.dx > 0.0f) || !(defaults.dt > 0.0f) || !(defaults.c > 0.0f) || courant > 0.7071f) {
        fail("WaveSim default Courant number r = " + std::to_string(courant) +
             " exceeds 2D CFL limit 0.7071 (dx=" + std::to_string(defaults.dx) +
             ", dt=" + std::to_string(defaults.dt) + ", c=" + std::to_string(defaults.c) + ")");
        return 1;
    }
    if (defaults.damping < 0.0f || defaults.damping >= 1.0f) {
        fail("WaveSim default damping " + std::to_string(defaults.damping) + " is outside [0, 1)");
        return 1;
    }

    std::cout << "parameters dx " << defaults.dx << " dt " << defaults.dt
              << " c " << defaults.c << " r " << courant
              << " damping " << defaults.damping << "\n";

    check_row_major_layout();
    check_linear(BoundaryMode::FixedDirichlet, "FixedDirichlet");
    check_linear(BoundaryMode::ReflectiveNeumann, "ReflectiveNeumann");
    if (failures != 0) {
        std::cerr << "wave stability: FAIL\n";
        return 1;
    }

    run_pool("checkerboard", BoundaryMode::FixedDirichlet, "Dirichlet", checkerboard, kCheckerPeakCeiling);
    run_pool("startup splashes", BoundaryMode::FixedDirichlet, "Dirichlet", startup_splashes, 2.5f);
    run_pool("click of 5", BoundaryMode::FixedDirichlet, "Dirichlet", mouse_click, 5.5f);
    run_pool("smooth splash", BoundaryMode::FixedDirichlet, "Dirichlet", smooth_splash, 12.0f);

    run_pool("checkerboard", BoundaryMode::ReflectiveNeumann, "Neumann", checkerboard, kCheckerPeakCeiling);
    run_pool("startup splashes", BoundaryMode::ReflectiveNeumann, "Neumann", startup_splashes, 2.5f);
    run_pool("click of 5", BoundaryMode::ReflectiveNeumann, "Neumann", mouse_click, 5.5f);
    run_pool("smooth splash", BoundaryMode::ReflectiveNeumann, "Neumann", smooth_splash, 12.0f);

    if (failures != 0) {
        std::cerr << "wave stability: FAIL\n";
        return 1;
    }

    sweep_corners_dirichlet();
    sweep_corners_neumann();
    if (failures != 0) {
        std::cerr << "wave stability: FAIL\n";
        return 1;
    }

    std::cout << "wave stability: PASS\n";
    return 0;
}
