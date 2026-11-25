// Runs the real update_wave from main.cpp. The step itself is not reimplemented here.
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

extern int width;
extern int height;
extern float dx;
extern float dt;
extern float c;
extern float damping;
extern std::vector<std::vector<float>> height_current;
extern std::vector<std::vector<float>> height_prev;
extern std::vector<std::vector<float>> height_next;

void update_wave();
void add_disturbance(int x, int y, float height);
void init_grid();

// 4x4, every sign pattern, 200 steps. update_wave is linear, so the corners
// cover every surface whose cells lie in [-1, 1].
static const int kCornerSteps = 200;
static const float kCornerPeakCeiling = 4.0f;
static const float kCornerFinalCeiling = 2.0f;

// 200x200 scenarios. The checkerboard is the stiffest ripple the stencil sees.
static const int kPoolSteps = 500;
static const float kCheckerPeakCeiling = 14.0f;
static const float kSettledHeight = 1.0f;

static int failures = 0;

static void fail(const std::string& message) {
    std::cerr << "FAIL " << message << "\n";
    ++failures;
}

static void use_grid(int n) {
    width = n;
    height = n;
    height_current.assign(n, std::vector<float>(n, 0.0f));
    height_prev.assign(n, std::vector<float>(n, 0.0f));
    height_next.assign(n, std::vector<float>(n, 0.0f));
}

static void zero_grids() {
    for (int i = 0; i < width; ++i) {
        for (int j = 0; j < height; ++j) {
            height_current[i][j] = 0.0f;
            height_prev[i][j] = 0.0f;
            height_next[i][j] = 0.0f;
        }
    }
}

static bool finite_grids() {
    for (int i = 0; i < width; ++i) {
        for (int j = 0; j < height; ++j) {
            if (!std::isfinite(height_current[i][j]) || !std::isfinite(height_prev[i][j])) {
                return false;
            }
        }
    }
    return true;
}

struct Cell {
    float value;
    int i;
    int j;
};

static Cell tallest() {
    Cell best{0.0f, 0, 0};
    for (int i = 0; i < width; ++i) {
        for (int j = 0; j < height; ++j) {
            float value = std::fabs(height_current[i][j]);
            if (value > std::fabs(best.value)) {
                best.value = height_current[i][j];
                best.i = i;
                best.j = j;
            }
        }
    }
    return best;
}

// Bits 0..15: height_current[i][j] for i * 4 + j. 1 is +1, 0 is -1.
// Bits 16..19: previous interior cells (1,1), (1,2), (2,1), (2,2).
static void load_corner(std::uint32_t mask) {
    zero_grids();
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            int bit = i * 4 + j;
            height_current[i][j] = (mask & (1u << bit)) ? 1.0f : -1.0f;
        }
    }
    const int interior[4][2] = {{1, 1}, {1, 2}, {2, 1}, {2, 2}};
    for (int k = 0; k < 4; ++k) {
        height_prev[interior[k][0]][interior[k][1]] =
            (mask & (1u << (16 + k))) ? 1.0f : -1.0f;
    }
}

static bool close_enough(float a, float b) {
    float scale = std::max(1.0f, std::max(std::fabs(a), std::fabs(b)));
    return std::fabs(a - b) <= 1e-4f * scale;
}

static void check_linear() {
    use_grid(4);
    load_corner(0x000abcdeu);
    std::vector<std::vector<float>> a_cur = height_current;
    std::vector<std::vector<float>> a_prev = height_prev;
    update_wave();
    std::vector<std::vector<float>> out_a = height_current;

    load_corner(0x00054321u);
    std::vector<std::vector<float>> b_cur = height_current;
    std::vector<std::vector<float>> b_prev = height_prev;
    update_wave();
    std::vector<std::vector<float>> out_b = height_current;

    zero_grids();
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            height_current[i][j] = a_cur[i][j] + b_cur[i][j];
            height_prev[i][j] = a_prev[i][j] + b_prev[i][j];
        }
    }
    update_wave();
    std::vector<std::vector<float>> out_sum = height_current;

    bool linear = true;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (!close_enough(out_sum[i][j], out_a[i][j] + out_b[i][j])) {
                linear = false;
            }
        }
    }
    if (!linear) {
        fail("update_wave is not linear, so the 4x4 corner sweep does not cover every surface in [-1, 1]");
    }

    zero_grids();
    update_wave();
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (height_current[i][j] != 0.0f) {
                fail("update_wave moved a flat zero surface at cell (" + std::to_string(i) + "," +
                     std::to_string(j) + ")");
                return;
            }
        }
    }
}

static void sweep_corners() {
    use_grid(4);
    float worst_peak = 0.0f;
    float worst_final = 0.0f;
    std::uint32_t peak_mask = 0;
    int peak_step = 0;
    const std::uint32_t corners = 1u << 20;

    for (std::uint32_t mask = 0; mask < corners; ++mask) {
        load_corner(mask);
        for (int step = 1; step <= kCornerSteps; ++step) {
            update_wave();
            if (!finite_grids()) {
                Cell cell = tallest();
                fail("4x4 corner mask " + std::to_string(mask) + " produced a non-finite height at update_wave step " +
                     std::to_string(step) + " cell (" + std::to_string(cell.i) + "," + std::to_string(cell.j) + ")");
                return;
            }
            Cell cell = tallest();
            float height = std::fabs(cell.value);
            if (height > kCornerPeakCeiling) {
                fail("4x4 corner mask " + std::to_string(mask) + ": after update_wave step " +
                     std::to_string(step) + ", cell (" + std::to_string(cell.i) + "," + std::to_string(cell.j) +
                     ") height " + std::to_string(cell.value) + " is above the ceiling " +
                     std::to_string(kCornerPeakCeiling));
                return;
            }
            if (height > worst_peak) {
                worst_peak = height;
                peak_mask = mask;
                peak_step = step;
            }
        }
        worst_final = std::max(worst_final, std::fabs(tallest().value));
    }

    if (worst_final > kCornerFinalCeiling) {
        fail("4x4 surfaces were still taller than " + std::to_string(kCornerFinalCeiling) + " after " +
             std::to_string(kCornerSteps) + " steps (tallest " + std::to_string(worst_final) + ")");
        return;
    }

    load_corner(peak_mask);
    Cell witness{0.0f, 0, 0};
    for (int step = 1; step <= peak_step; ++step) {
        update_wave();
        witness = tallest();
    }
    std::cout << "4x4 every surface in [-1, 1]: tallest height " << std::fabs(witness.value)
              << " at step " << peak_step << " cell (" << witness.i << "," << witness.j
              << ") corner mask " << peak_mask << ". At step " << kCornerSteps << " the tallest was "
              << worst_final << ".\n";
}

static void run_pool(const char* name, void (*setup)(), float peak_ceiling) {
    use_grid(400);
    zero_grids();
    setup();
    float peak = std::fabs(tallest().value);
    int peak_step = 0;
    Cell peak_cell = tallest();
    for (int step = 1; step <= kPoolSteps; ++step) {
        update_wave();
        if (!finite_grids()) {
            Cell cell = tallest();
            fail(std::string(name) + ": non-finite height after update_wave step " + std::to_string(step) +
                 " cell (" + std::to_string(cell.i) + "," + std::to_string(cell.j) + ")");
            return;
        }
        Cell cell = tallest();
        float height = std::fabs(cell.value);
        if (height > peak_ceiling) {
            fail(std::string(name) + ": after update_wave step " + std::to_string(step) + ", cell (" +
                 std::to_string(cell.i) + "," + std::to_string(cell.j) + ") height " + std::to_string(cell.value) +
                 " is above the ceiling " + std::to_string(peak_ceiling));
            return;
        }
        if (height > peak) {
            peak = height;
            peak_step = step;
            peak_cell = cell;
        }
    }
    Cell final_cell = tallest();
    if (std::fabs(final_cell.value) >= kSettledHeight) {
        fail(std::string(name) + ": after update_wave step " + std::to_string(kPoolSteps) + ", cell (" +
             std::to_string(final_cell.i) + "," + std::to_string(final_cell.j) + ") height " +
             std::to_string(final_cell.value) + " has not fallen below " + std::to_string(kSettledHeight));
        return;
    }
    std::cout << name << ": peak height " << peak << " at step " << peak_step << " cell (" << peak_cell.i << ","
              << peak_cell.j << "). At step " << kPoolSteps << " the tallest is " << final_cell.value << ".\n";
}

static void startup_splashes() {
    init_grid();
    add_disturbance(100, 100, 2.0f);
    add_disturbance(300, 300, 1.5f);
    add_disturbance(150, 250, 1.8f);
}

static void mouse_click() {
    init_grid();
    add_disturbance(200, 200, 5.0f);
}

static void checkerboard() {
    for (int i = 1; i < width - 1; ++i) {
        for (int j = 1; j < height - 1; ++j) {
            float sign = ((i + j) % 2 == 0) ? 1.0f : -1.0f;
            height_current[i][j] = sign;
            height_prev[i][j] = sign;
        }
    }
}

int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    if (width != 400 || height != 400) {
        fail("main.cpp grid is " + std::to_string(width) + "x" + std::to_string(height) +
             ", not 400x400. The pool scenarios and ceilings no longer match the code.");
        return 1;
    }

    std::cout << "parameters dx " << dx << " dt " << dt << " c " << c << " damping " << damping << "\n";
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
