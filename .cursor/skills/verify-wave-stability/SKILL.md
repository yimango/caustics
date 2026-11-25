---
name: verify-wave-stability
description: "Check that update_wave in main.cpp keeps the water surface finite and bounded. Use when changing the wave step, add_disturbance, init_grid, or dx, dt, c, damping, width, or height."
---

# Verify wave stability

A water surface must stay a finite height, and a sharp ripple must fall back below 1 within 500 steps of `update_wave`. If this fails, the mesh heights run away and the caustic pass draws garbage. Nothing else in the repo runs this step.

## When to run

Run it after any edit to `update_wave`, `add_disturbance`, `init_grid`, or the globals `width`, `height`, `dx`, `dt`, `c`, and `damping` in `main.cpp`. Also run it if the three startup `add_disturbance` calls or the click height `5.0f` change.

## Run

From the repo root:

```
powershell -ExecutionPolicy Bypass -File verify/check.ps1
```

The script needs `g++` and `gcc` (confirmed with MinGW g++ 15.1.0) and `opengl32`. It rebuilds `main.cpp` and runs `verify/wave_stability.exe`. A good run takes about 14 seconds and ends with `wave stability: PASS`. Exit 0 is a pass.

`verify/wave_stability.cpp` calls `update_wave`, `add_disturbance`, and `init_grid`. It does not reimplement the step. `-Dmain=caustics_main` renames `main` so the check can link. `verify/stubs/glm` exists so the checker can compile without the real GLM tree. `verify/glfw_link_stubs.c` exists because the GLFW import library is gitignored under `**/lib-*/`. The check never calls GLFW or GLM.

## What a pass covers

`update_wave` is linear in the current and previous heights. The check tests that on two surfaces and on a flat zero pool. While that holds, every 4×4 surface whose cells lie in [-1, 1] is covered by the 1,048,576 sign patterns (16 current cells, plus the 4 previous interior cells). Boundary previous cells are never read. Each pattern runs 200 steps.

The 400×400 pool is not exhaustive. The check runs three surfaces on it, for 500 steps:

- A checkerboard of height 1 on the interior, previous frame equal to the current frame.
- The startup splashes in `main`: height 2 at (100,100), 1.5 at (300,300), 1.8 at (150,250).
- A click of height 5 at (200,200), the height `mouse_button_callback` writes.

`init_grid` zeros all three height grids. The driver also zeros them before each scenario.

Ceilings, and the heights witnessed on the passing run:

| Scenario | Ceiling | Witnessed |
|---|---|---|
| 4×4, any step in the first 200 | 4 | 3.42989 at step 33, cell (2,2), corner mask 93572 |
| 4×4, height at step 200 | 2 | 1.3432 |
| Checkerboard peak | 14 | 12.8534 at step 19, cell (11,11) |
| Checkerboard at step 500 | below 1 | about 0.999 |
| Startup splashes | 2, and below 1 at step 500 | peak 2 at the splash |
| Click of 5 | 5, and below 1 at step 500 | peak 5 at the click |

Parameters witnessed: `dx` 1, `dt` 0.7, `c` 1, `damping` 0.01, grid 400×400.

## How to read a failure

`FAIL` lines are the result. A height above its ceiling, a non-finite height, or a pool that is still at height 1 or above after 500 steps is a bug in the wave step.

Do not raise a ceiling to silence a failure. If `dt`, `c`, `dx`, `damping`, or the grid size was retuned on purpose, re-run the check, write the new peaks into the table, set each ceiling just above the peak, and only then move the sync commit.

`update_wave is not linear` means the corner sweep no longer covers the box. Read `update_wave` before trusting the three pool scenarios.

## Coverage

| Code | What the check calls |
|---|---|
| `main.cpp` globals `width`, `height`, `dx`, `dt`, `c`, `damping`, and the three height grids | Read and resized. The 4×4 sweep sets `width` and `height` to 4, then the pool scenarios set them to 400. |
| `update_wave` | Every step of every scenario |
| `add_disturbance`, `init_grid` | Startup splashes and the click |

Sync commit: update this after the 400-grid change is committed. Last confirmed green against the working tree that uses `width = height = 400`.

## Not covered

Shaders, the framebuffer pass, refraction, mouse-to-grid mapping, and mesh height exaggeration (`* 8.0f` in `generateWaterMesh`) are not run. Horizons past 200 steps on the 4×4 grid and past 500 steps on the pool are not checked. Rim clicks are rejected by `add_disturbance` and are not part of this check.
