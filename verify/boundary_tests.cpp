#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "sim/wave_sim.h"

namespace {

int g_failures = 0;

void fail(const std::string& msg) {
    std::cerr << "FAIL " << msg << "\n";
    ++g_failures;
}

bool all_finite(const WaveSim& sim) {
    const int n = sim.width * sim.height;
    for (int i = 0; i < n; ++i) {
        if (!std::isfinite(sim.height_current[i]) || !std::isfinite(sim.height_prev[i])) {
            return false;
        }
    }
    return true;
}

double trapezoidal_volume(const WaveSim& sim) {
    double sum = 0.0;
    for (int y = 0; y < sim.height; ++y) {
        const double wy = (y == 0 || y == sim.height - 1) ? 0.5 : 1.0;
        for (int x = 0; x < sim.width; ++x) {
            const double wx = (x == 0 || x == sim.width - 1) ? 0.5 : 1.0;
            sum += wx * wy * static_cast<double>(sim.height_current[sim.idx(x, y)]);
        }
    }
    return sum;
}

double wave_energy(const WaveSim& sim) {
    const double inv_dt = 1.0 / static_cast<double>(sim.dt);
    const double inv_dx = 1.0 / static_cast<double>(sim.dx);
    const double c2 = static_cast<double>(sim.c) * static_cast<double>(sim.c);

    double kinetic = 0.0;
    for (int y = 0; y < sim.height; ++y) {
        const double wy = (y == 0 || y == sim.height - 1) ? 0.5 : 1.0;
        for (int x = 0; x < sim.width; ++x) {
            const double wx = (x == 0 || x == sim.width - 1) ? 0.5 : 1.0;
            const int i = sim.idx(x, y);
            const double vel = (static_cast<double>(sim.height_current[i]) -
                                static_cast<double>(sim.height_prev[i])) * inv_dt;
            kinetic += 0.5 * wx * wy * vel * vel;
        }
    }

    double potential = 0.0;
    for (int y = 0; y < sim.height; ++y) {
        const double wy = (y == 0 || y == sim.height - 1) ? 0.5 : 1.0;
        for (int x = 0; x < sim.width - 1; ++x) {
            const double grad_x = (static_cast<double>(sim.height_current[sim.idx(x + 1, y)]) -
                                   static_cast<double>(sim.height_current[sim.idx(x, y)])) * inv_dx;
            potential += 0.5 * c2 * wy * grad_x * grad_x;
        }
    }
    for (int y = 0; y < sim.height - 1; ++y) {
        for (int x = 0; x < sim.width; ++x) {
            const double wx = (x == 0 || x == sim.width - 1) ? 0.5 : 1.0;
            const double grad_y = (static_cast<double>(sim.height_current[sim.idx(x, y + 1)]) -
                                   static_cast<double>(sim.height_current[sim.idx(x, y)])) * inv_dx;
            potential += 0.5 * c2 * wx * grad_y * grad_y;
        }
    }
    return kinetic + potential;
}

void launch_leftward_packet(WaveSim& sim, float x0, float y0, float sigma_x, float sigma_y, float amp) {
    const float inv_2sx2 = 1.0f / (2.0f * sigma_x * sigma_x);
    const float inv_2sy2 = 1.0f / (2.0f * sigma_y * sigma_y);
    const float prev_x0 = x0 + sim.c * sim.dt;
    for (int y = 0; y < sim.height; ++y) {
        const float dy = static_cast<float>(y) - y0;
        const float ey = std::exp(-(dy * dy) * inv_2sy2);
        for (int x = 0; x < sim.width; ++x) {
            const float dx_cur = static_cast<float>(x) - x0;
            const float dx_prev = static_cast<float>(x) - prev_x0;
            const int i = sim.idx(x, y);
            sim.height_current[i] = amp * std::exp(-(dx_cur * dx_cur) * inv_2sx2) * ey;
            sim.height_prev[i] = amp * std::exp(-(dx_prev * dx_prev) * inv_2sx2) * ey;
        }
    }
}

void test_neumann_in_phase_reflection() {
    WaveSim sim;
    sim.dx = 1.0f;
    sim.c = 1.0f;
    sim.dt = 0.5f;
    sim.damping = 0.0f;
    sim.boundaryMode = BoundaryMode::ReflectiveNeumann;
    sim.init(128, 128);

    const int cy = 64;
    launch_leftward_packet(sim, 32.0f, static_cast<float>(cy), 4.0f, 16.0f, 1.0f);

    float pre_wall_peak = 0.0f;
    float wall_peak = 0.0f;
    int wall_peak_step = 0;

    for (int step = 1; step <= 120; ++step) {
        sim.step();
        if (!all_finite(sim)) {
            fail("neumann_reflection: non-finite values at step " + std::to_string(step));
            return;
        }
        if (step == 20) {
            for (int x = 5; x < 64; ++x) {
                pre_wall_peak = std::max(pre_wall_peak, sim.height_current[sim.idx(x, cy)]);
            }
        }
        const float h_wall = sim.height_current[sim.idx(0, cy)];
        if (h_wall > wall_peak) {
            wall_peak = h_wall;
            wall_peak_step = step;
        }
    }

    float reflected_pos = 0.0f;
    float reflected_neg_mag = 0.0f;
    for (int x = 10; x <= 55; ++x) {
        const float v = sim.height_current[sim.idx(x, cy)];
        if (v > reflected_pos) reflected_pos = v;
        if (-v > reflected_neg_mag) reflected_neg_mag = -v;
    }

    std::cout << "neumann_reflection: pre_wall_peak=" << pre_wall_peak
              << " wall_peak=" << wall_peak << " (step " << wall_peak_step << ")"
              << " reflected_pos=" << reflected_pos
              << " reflected_neg_mag=" << reflected_neg_mag << "\n";

    if (!(wall_peak > 0.5f) || !(wall_peak > pre_wall_peak * 1.15f)) {
        fail("neumann_reflection: wall run-up failed (wall_peak=" + std::to_string(wall_peak) +
             ", pre_wall_peak=" + std::to_string(pre_wall_peak) + ")");
    }
    if (!(reflected_pos > 0.35f) || !(reflected_pos > reflected_neg_mag * 2.0f)) {
        fail("neumann_reflection: reflected packet did not preserve positive polarity (pos=" +
             std::to_string(reflected_pos) + ", neg_mag=" + std::to_string(reflected_neg_mag) + ")");
    }
}

void test_volume_conservation() {
    WaveSim sim;
    sim.dx = 1.0f;
    sim.c = 1.0f;
    sim.dt = 0.5f;
    sim.damping = 0.0f;
    sim.boundaryMode = BoundaryMode::ReflectiveNeumann;
    sim.init(100, 100);

    // Release a localized positive hump from rest (height_prev == height_current => zero net momentum)
    // near two walls so reflections occur repeatedly over 300 steps.
    const float cx = 20.0f;
    const float cy = 25.0f;
    const float sigma = 5.0f;
    const float inv_2s2 = 1.0f / (2.0f * sigma * sigma);
    for (int y = 0; y < sim.height; ++y) {
        for (int x = 0; x < sim.width; ++x) {
            const float dx = static_cast<float>(x) - cx;
            const float dy = static_cast<float>(y) - cy;
            const float v = 2.0f * std::exp(-(dx * dx + dy * dy) * inv_2s2);
            const int i = sim.idx(x, y);
            sim.height_current[i] = v;
            sim.height_prev[i] = v;
        }
    }

    const double vol0 = trapezoidal_volume(sim);
    double max_rel_err = 0.0;
    for (int step = 1; step <= 300; ++step) {
        sim.step();
        if (!all_finite(sim)) {
            fail("volume_conservation: non-finite values at step " + std::to_string(step));
            return;
        }
        const double vol = trapezoidal_volume(sim);
        const double rel_err = std::fabs(vol - vol0) / std::max(1e-9, std::fabs(vol0));
        max_rel_err = std::max(max_rel_err, rel_err);
    }

    std::cout << "volume_conservation: vol0=" << vol0
              << " final_vol=" << trapezoidal_volume(sim)
              << " max_rel_err=" << max_rel_err << "\n";

    if (max_rel_err >= 1e-3) {
        fail("volume_conservation: relative volume drift " + std::to_string(max_rel_err) + " >= 1e-3");
    }
}

void test_corner_diagonal_symmetry() {
    WaveSim sim;
    sim.dx = 1.0f;
    sim.c = 1.0f;
    sim.dt = 0.5f;
    sim.damping = 0.008f;
    sim.boundaryMode = BoundaryMode::ReflectiveNeumann;
    sim.init(100, 100);

    sim.addDisturbance(15, 15, 3.0f, 4.0f);

    for (int step = 1; step <= 100; ++step) {
        sim.step();
        if (!all_finite(sim)) {
            fail("corner_symmetry: non-finite values at step " + std::to_string(step));
            return;
        }
    }

    float max_asym = 0.0f;
    float max_abs = 0.0f;
    for (int y = 0; y < sim.height; ++y) {
        for (int x = 0; x < sim.width; ++x) {
            const float a = sim.height_current[sim.idx(x, y)];
            const float b = sim.height_current[sim.idx(y, x)];
            max_asym = std::max(max_asym, std::fabs(a - b));
            max_abs = std::max(max_abs, std::fabs(a));
        }
    }

    std::cout << "corner_symmetry: max_asym=" << max_asym << " max_abs=" << max_abs << "\n";

    if (max_asym >= 1e-5f) {
        fail("corner_symmetry: diagonal asymmetry " + std::to_string(max_asym) + " >= 1e-5");
    }
    if (max_abs < 0.01f) {
        fail("corner_symmetry: wave vanished unexpectedly (max_abs=" + std::to_string(max_abs) + ")");
    }
}

void test_partial_absorb_robin() {
    WaveSim neumann;
    neumann.dx = 1.0f;
    neumann.c = 1.0f;
    neumann.dt = 0.5f;
    neumann.damping = 0.0f;
    neumann.boundaryMode = BoundaryMode::ReflectiveNeumann;
    neumann.init(128, 128);

    WaveSim robin = neumann;
    robin.boundaryMode = BoundaryMode::PartialAbsorbRobin;
    robin.wallReflection = 0.5f;

    const int cy = 64;
    launch_leftward_packet(neumann, 32.0f, static_cast<float>(cy), 4.0f, 16.0f, 1.0f);
    launch_leftward_packet(robin, 32.0f, static_cast<float>(cy), 4.0f, 16.0f, 1.0f);

    for (int step = 1; step <= 120; ++step) {
        neumann.step();
        robin.step();
        if (!all_finite(neumann) || !all_finite(robin)) {
            fail("partial_absorb_robin: non-finite values at step " + std::to_string(step));
            return;
        }
    }

    float neumann_pos = 0.0f;
    float robin_pos = 0.0f;
    float robin_neg_mag = 0.0f;
    for (int x = 10; x <= 55; ++x) {
        neumann_pos = std::max(neumann_pos, neumann.height_current[neumann.idx(x, cy)]);
        const float rv = robin.height_current[robin.idx(x, cy)];
        if (rv > robin_pos) robin_pos = rv;
        if (-rv > robin_neg_mag) robin_neg_mag = -rv;
    }

    const double e_neumann = wave_energy(neumann);
    const double e_robin = wave_energy(robin);

    std::cout << "partial_absorb_robin: neumann_pos=" << neumann_pos
              << " robin_pos=" << robin_pos << " robin_neg_mag=" << robin_neg_mag
              << " e_robin/e_neumann=" << (e_neumann > 0.0 ? e_robin / e_neumann : 0.0) << "\n";

    if (!(neumann_pos > 0.3f)) {
        fail("partial_absorb_robin: Neumann reference reflection too small (" + std::to_string(neumann_pos) + ")");
    }
    if (!(robin_pos < neumann_pos * 0.95f) || !(e_robin < e_neumann * 0.92)) {
        fail("partial_absorb_robin: Robin reflection was not strictly attenuated vs Neumann");
    }
    if (!(robin_pos > 0.05f) || !(robin_pos > robin_neg_mag)) {
        fail("partial_absorb_robin: Robin reflection should remain positive/non-inverted (pos=" +
             std::to_string(robin_pos) + ", neg_mag=" + std::to_string(robin_neg_mag) + ")");
    }
}

void test_absorbing_open() {
    WaveSim neumann;
    neumann.dx = 1.0f;
    neumann.c = 1.0f;
    neumann.dt = 0.5f;
    neumann.damping = 0.0f;
    neumann.boundaryMode = BoundaryMode::ReflectiveNeumann;
    neumann.init(128, 128);

    WaveSim open_sim = neumann;
    open_sim.boundaryMode = BoundaryMode::AbsorbingOpen;

    launch_leftward_packet(neumann, 28.0f, 64.0f, 4.0f, 20.0f, 1.0f);
    launch_leftward_packet(open_sim, 28.0f, 64.0f, 4.0f, 20.0f, 1.0f);

    for (int step = 1; step <= 130; ++step) {
        neumann.step();
        open_sim.step();
        if (!all_finite(neumann) || !all_finite(open_sim)) {
            fail("absorbing_open: non-finite values at step " + std::to_string(step));
            return;
        }
    }

    const double e_neumann = wave_energy(neumann);
    const double e_open = wave_energy(open_sim);
    const double remain_ratio = (e_neumann > 0.0) ? (e_open / e_neumann) : 1.0;

    std::cout << "absorbing_open: e_neumann=" << e_neumann
              << " e_open=" << e_open
              << " remaining_ratio=" << remain_ratio
              << " absorbed_pct=" << (1.0 - remain_ratio) * 100.0 << "%\n";

    if (!(e_neumann > 0.0) || remain_ratio >= 0.15) {
        fail("absorbing_open: remaining energy ratio " + std::to_string(remain_ratio) +
             " >= 0.15 (expected > 85% absorption vs ReflectiveNeumann)");
    }
}

void test_wall_adjacent_drop_all_modes() {
    const struct {
        BoundaryMode mode;
        const char* name;
    } modes[] = {
        {BoundaryMode::ReflectiveNeumann, "ReflectiveNeumann"},
        {BoundaryMode::PartialAbsorbRobin, "PartialAbsorbRobin"},
        {BoundaryMode::AbsorbingOpen, "AbsorbingOpen"},
        {BoundaryMode::FixedDirichlet, "FixedDirichlet"},
    };

    for (const auto& entry : modes) {
        WaveSim sim;
        sim.dx = 1.0f;
        sim.c = 1.0f;
        sim.dt = 0.5f;
        sim.damping = 0.01f;
        sim.boundaryMode = entry.mode;
        sim.init(400, 400);

        sim.addDisturbance(2, 200, 3.0f, 4.0f);

        float max_abs = 0.0f;
        float max_grad = 0.0f;
        float max_wall = 0.0f;
        for (int step = 1; step <= 200; ++step) {
            sim.step();
            if (!all_finite(sim)) {
                fail(std::string("wall_adjacent_drop [") + entry.name + "]: non-finite at step " +
                     std::to_string(step));
                return;
            }
            max_wall = std::max(max_wall, std::fabs(sim.height_current[sim.idx(0, 200)]));
            for (int y = 0; y < sim.height; ++y) {
                for (int x = 0; x < sim.width; ++x) {
                    const float h = sim.height_current[sim.idx(x, y)];
                    max_abs = std::max(max_abs, std::fabs(h));
                    if (x + 1 < sim.width) {
                        max_grad = std::max(max_grad, std::fabs(sim.height_current[sim.idx(x + 1, y)] - h));
                    }
                    if (y + 1 < sim.height) {
                        max_grad = std::max(max_grad, std::fabs(sim.height_current[sim.idx(x, y + 1)] - h));
                    }
                }
            }
        }

        std::cout << "wall_adjacent_drop [" << entry.name << "]: max_abs=" << max_abs
                  << " max_grad=" << max_grad << " max_wall_h=" << max_wall << "\n";

        if (max_abs > 12.0f || max_grad > 12.0f) {
            fail(std::string("wall_adjacent_drop [") + entry.name + "]: unbounded or non-smooth response (max_abs=" +
                 std::to_string(max_abs) + ", max_grad=" + std::to_string(max_grad) + ")");
        }
        if (entry.mode == BoundaryMode::ReflectiveNeumann && max_wall < 0.1f) {
            fail("wall_adjacent_drop [ReflectiveNeumann]: wall cell (0,200) did not respond to drop at (2,200)");
        }
    }
}

} // namespace

int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    test_neumann_in_phase_reflection();
    test_volume_conservation();
    test_corner_diagonal_symmetry();
    test_partial_absorb_robin();
    test_absorbing_open();
    test_wall_adjacent_drop_all_modes();

    if (g_failures != 0) {
        std::cerr << "boundary tests: FAIL (" << g_failures << " failure(s))\n";
        return 1;
    }
    std::cout << "boundary tests: PASS\n";
    return 0;
}
