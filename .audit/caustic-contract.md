# Caustic Interface & Boundary Physics Contract

Architecture and invariants for the modular C++17 / OpenGL 3.3 water caustics simulator.

## Module Architecture

| Module | Files | Responsibility |
| --- | --- | --- |
| Pure Simulation | `src/sim/wave_sim.{h,cpp}` | 2D shallow-water heightfield solver (`WaveSim`, `BoundaryMode`, `WindParams`). Zero OpenGL/GLFW/GLM dependencies. |
| Coordinate & Optics SSOT | `src/render/pool_coords.h`, `src/render/pool_optics.h` | Exact grid/world/UV mapping (`PoolConfig`), ray-pool-box intersection, 5-face unfolded caustic atlas UV mapping, analytical rim shadow occlusion. |
| Render State & Shaders | `src/render/scene_state.h`, `src/render/shader_program.{h,cpp}`, `src/render/shaders/*.h` | Shared POD structs (`LightConfig`, `RenderSettings`, `SceneTargets`, `CameraState`), cached uniform shader wrapper, modular GLSL sources (`common_height.h`, `caustics_shaders.h`, `pool_shaders.h`, `water_shaders.h`, `skybox_shaders.h`, `overlay_shaders.h`). |
| Caustics Pass | `src/render/caustics_pass.{h,cpp}` | R32F height texture upload, 5-face unfolded caustic FBO (`GL_RGB16F`/`GL_RGBA16F`), crisp separable blur. |
| Pool Renderer | `src/render/pool_renderer.{h,cpp}` | Floor + 4 vertical walls with true inward normals, CCW inward winding, dynamic waterline, submerged caustics, dry stone shading, rim shadows. |
| Water Renderer | `src/render/water_renderer.{h,cpp}` | Water mesh (`[-hw, +hw] x [-hh, +hh]`), analytical Catmull-Rom normals, capillary wall meniscus, real camera-ray underwater refraction (via `SceneTargets`), Beer-Lambert absorption, Fresnel sky + pool-wall reflections. |
| Skybox Renderer | `src/render/skybox_renderer.{h,cpp}` | Sky gradient background pass. |
| Camera & HUD | `src/ui/camera.{h,cpp}`, `src/ui/overlay_ui.{h,cpp}` | Orbit/pan/zoom camera with presets 1-4; single-draw-call batched overlay UI and 5x7 bitmap font. |
| Application | `src/app.{h,cpp}`, `main.cpp` | GLFW lifecycle, HiDPI/resize handling, offscreen `SceneTargets` FBO, CLI flags, BMP capture, fixed-timestep loop. |

## Pool Coordinates (`src/render/pool_coords.h`)

`PoolConfig` is the single source of truth shared across CPU geometry, raycasting, and GPU height sampling:

| Field / Method | Default / Formula |
| --- | --- |
| `gridW`, `gridH` | `400`, `400` (supports rectangular pools) |
| `waterScale` | `1.0f` |
| `halfWidth()`, `halfHeight()` | `0.5f * gridW * waterScale` (`200.0f`), `0.5f * gridH * waterScale` (`200.0f`) |
| `bottomZ`, `lipZ` | `-30.0f`, `18.0f` (wall height `H = lipZ - bottomZ = 48.0f`) |
| `meshHeightScale` | `8.0f` |
| `gridToWorldX(gx)` | `-hw + (gx / float(gridW - 1)) * (2.0f * hw)` |
| `worldToGridX(x)` | `((x + hw) / (2.0f * hw)) * float(gridW - 1)` |
| `worldToUV(x, y)` | `((x + hw) / (2.0f * hw), (y + hh) / (2.0f * hh))` |

This bijection maps `gx = 0` to `-hw` and `gx = gridW - 1` to `+hw` identically on both axes, eliminating the legacy 1-unit east/north water-wall gap and `+200` texel overshoot.

## Height Storage & Wave Solver (`src/sim/wave_sim.{h,cpp}`)

- **Storage layout**: Canonical row-major flat vectors (`height_current`, `height_prev`, `height_next`) indexed by `idx(x, y) = y * width + x`. Uploaded directly as a `width x height` `GL_R32F` texture with zero X/Y transposition.
- **Velocity-damped wave update** (CFL-safe default `dx = 1.0f`, `c = 1.0f`, `dt = 0.5f`, `r = c * dt / dx = 0.5 <= 0.7071`, `damping = 0.008f`):

```text
laplacian = E + W + N + S - 4 * C
height_next[i] = C + (1 - damping) * (C - P) + (c * dt / dx)^2 * laplacian
```

- **Buffer rotation**: Every cell (interior, edges, and corners) is written into `height_next` each step, allowing O(1) three-buffer `std::swap` rotation.
- **Boundary modes (`BoundaryMode`)**:
  - `ReflectiveNeumann` (default): 2nd-order symmetric ghost-cell Laplacian (`h(-1, y) = h(1, y)`, etc.) on all 4 edges and corners. Preserves in-phase (+1) wave reflection with ~2x wall run-up, diagonal corner symmetry, and strict trapezoidal-weighted volume conservation when `damping == 0`.
  - `PartialAbsorbRobin`: Neumann stencil with perimeter dissipation controlled by `wallReflection` in `[0, 1]`. Attenuates reflected energy without inverting wave phase.
  - `AbsorbingOpen`: 1st-order Mur absorbing boundary condition with corner averaging plus a cosine-tapered sponge damping layer to absorb outgoing waves (>85% energy exit).
  - `FixedDirichlet`: Legacy pinned zero boundary (`height_next = 0` on border).
- **Splashes & wind**: `addDisturbance(x, y, amplitude, radius)` defaults to a smooth additive Gaussian kernel (`radius = 4.0f`, single-cell additive impulse when `radius <= 0.5f`) that mirror-folds near reflective walls and initializes zero vertical velocity (`height_current` and `height_prev`) to preserve volume without step-1 overshoot. `applyWind(simTime, wind)` drives multi-directional traveling wind waves smoothly across the domain.
- **Helpers**: `reset()`, `totalVolume()`, `totalEnergy()`, and `stepFixed(deltaSeconds, accumulator, maxSubsteps)`.

## Analytical Height Derivatives & 5-Face Unfolded Caustic Atlas

### Analytical Catmull-Rom Derivatives (`common_height.h`, `water_shaders.h`)

`sampleHeightAndGrad(vec2 xy, out float h, out vec2 grad)` evaluates both interpolated height `h` and exact analytical world gradient `(dh/dx, dh/dy)` in a single 4x4 (16-tap) Catmull-Rom window, scaling index derivatives by `(gridSize - 1.0) / (poolHalfExtent * 2.0)`. `sampleHeight` and `heightSlope` remain available for compatibility.

### Ray-Pool-Box Intersection & Unfolded Cross Atlas (`pool_optics.h`, `caustics_pass.*`, `caustics_shaders.h`)

Refracted rays from the water surface (`airIOR = 1.0`, `waterIOR = 1.33`, optional RGB chromatic dispersion via `settings.dispersion`) are intersected against the 5 interior faces of the pool box (`[-hw, +hw] x [-hh, +hh] x [bottomZ, lipZ]`):

- Face `0`: Floor (`z = bottomZ`, normal `(0, 0, +1)`) -> unfolded `(x, y)`
- Face `1`: South wall (`y = -hh`, normal `(0, +1, 0)`) -> unfolded `(x, -hh - (z - bottomZ))`
- Face `2`: East wall (`x = +hw`, normal `(-1, 0, 0)`) -> unfolded `(+hw + (z - bottomZ), y)`
- Face `3`: North wall (`y = +hh`, normal `(0, -1, 0)`) -> unfolded `(x, +hh + (z - bottomZ))`
- Face `4`: West wall (`x = -hw`, normal `(+1, 0, 0)`) -> unfolded `(-hw - (z - bottomZ), y)`

With wall height `H = lipZ - bottomZ`, unfolded coordinates map linearly into `[0, 1]^2`:

```text
uv = (unfolded + vec2(hw + H, hh + H)) / vec2(2 * hw + 2 * H, 2 * hh + 2 * H)
```

Because each wall strip attaches directly along its shared floor edge at `z = bottomZ`, floor-to-wall caustic projection is continuous across all four floor-wall creases. Wall-to-wall corner seam triangles are discarded to prevent stretched artifacts. Flux is computed from fragment area ratios (`dFdx`/`dFdy`), weighted by Fresnel transmittance and incident cosine, accumulated into a 16-bit floating-point FBO (`GL_RGB16F`/`GL_RGBA16F`), and filtered with a tight crisp separable Gaussian blur.

### Analytical Rim Shadows (`pool_optics.h`, `caustics_shaders.h`, `pool_shaders.h`)

A ray from surface point `P` toward `lightPos` is tested against the four wall planes `x = ±hw`, `y = ±hh`. If it exits the pool footprint at `z_exit < lipZ` before reaching the light (`t < 1`), the point is shadowed by the pool rim (with a smooth penumbra band near `z_exit ≈ lipZ`). Rim occlusion gates both caustic injection and direct lighting.

## Dynamic Waterline, Pool Shading & Water Surface Realism

- **Pool walls (`pool_shaders.h`)**: Every wall fragment samples live `waterZ = sampleHeight(clampedWallXY) * heightScale`:
  - Submerged (`FragPos.z < waterZ`): samples wall caustics from the 5-face unfolded atlas, applies wet stone darkening, Beer-Lambert depth tint, and corner ambient occlusion.
  - Waterline meniscus (`abs(FragPos.z - waterZ)` near 0): subtle wet specular contact highlight tracking moving waves.
  - Dry wall (`FragPos.z >= waterZ`): dry masonry Lambertian shading with rim shadows and zero caustic bleed.
- **Water surface (`water_renderer.*`, `water_shaders.h`)**:
  - Covers `[-hw, +hw] x [-hh, +hh]` seamlessly (watertight perimeter overlap/skirt so sloshing waves never expose sky gaps).
  - Applies a subtle capillary meniscus normal curve within ~2.5 world units of each wall.
  - When `SceneTargets` (`colorTex` + `depthTex` rendered offscreen in `App`) is provided, traces transmitted refracted rays into the pool box, projects underwater hits to screen UV with depth-buffer validation (preventing dry-wall foreground bleed), supports optional RGB dispersion, applies Beer-Lambert depth absorption, and reflects above-water dry pool walls or sky via Schlick Fresnel. Falls back cleanly to translucent shading when `SceneTargets` is null.

## Interactive Controls & CLI

- **Camera**: Right-drag orbit, Middle-drag / Shift+Right-drag pan, Scroll zoom, Presets `1` (Top-down), `2` (Classic oblique), `3` (Low corner inspection), `4` (Grazing side view).
- **Lighting & Sim**: `L` + Left-drag moves light; `Space` pause/resume; `R` reset water; `B` cycle boundary mode; `H` toggle HUD; `Escape` quit. Left-click on water adds a smooth drop.
- **HUD**: Single-draw-call batched overlay anchored in logical pixels (HiDPI/resize-safe) with `AMP`, `SCALE`, `TEMPO`, `REFLECT`, `LIGHT EL`, `LIGHT AZ`, `DISP` sliders and a clickable Boundary Mode button.
- **CLI flags**:
  - `--dump <path.bmp> [frames]` (default 180 frames; deterministic 1 sim step/frame; disables vsync for fast capture)
  - `--wind <amp> <wavelength> <tempo>`
  - `--drop <gx> <gy> <amp> <frame>`
  - `--camera <ex> <ey> <ez> <tx> <ty> <tz>`
  - `--preset <1|2|3|4>`
  - `--light <x> <y> <z>`
  - `--boundary <reflective|robin|absorb|dirichlet>`
  - `--dispersion <val>`
  - `--no-ui`

## Verification Suite

| Check | Command | Coverage |
| --- | --- | --- |
| Full C++ verification | `powershell -ExecutionPolicy Bypass -File verify/check.ps1` | Builds and runs `wave_stability`, `optics_tests`, and `boundary_tests` (`-SkipBoundary` optional). |
| Wave stability | `verify/wave_stability.exe` | Row-major layout, CFL check, linearity, 400x400 scenarios, and 2^20 4x4 sweeps for both `FixedDirichlet` and `ReflectiveNeumann`. |
| Boundary physics | `verify/boundary_tests.exe` | Neumann in-phase reflection & run-up, trapezoidal volume conservation (<1e-3), corner diagonal symmetry (<1e-5), Robin attenuation, AbsorbingOpen >85% energy exit, wall-adjacent drops across all 4 modes. |
| Optics & coordinates | `verify/optics_tests.exe` | Square & rectangular `PoolConfig` bijection, `poolBoxHit` 5-face classification, floor-to-wall unfolded UV continuity, rim shadow occlusion. |
| Floor ring check | `python verify/ring_band.py <frame.bmp> [--preset classic|topdown] [--eye ...] [--center ...] [--up ...] [--fov ...]` | Annulus caustic band connectivity and zero white clipping. |
| Wall caustics check | `python verify/wall_caustics_check.py <frame.bmp> [--preset 3]` | Submerged wall 2D caustic contrast, dry wall cleanliness, zero white clipping, and zero water-wall seam sky bleed. |
