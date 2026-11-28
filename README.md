# Water Caustics Simulator

Real-time C++17 / OpenGL 3.3 shallow-water wave and caustics simulator featuring physically-based boundary hydrodynamics, 5-face floor-and-wall caustics, analytical rim shadows, dynamic waterline shading, and camera-ray underwater refraction.

## Highlights

- **Modular architecture**: Pure C++17 wave simulation (`src/sim/wave_sim.{h,cpp}` with zero GL/GLFW/GLM dependencies), shared coordinate & optical geometry SSOT (`src/render/pool_coords.h`, `src/render/pool_optics.h`), modular OpenGL render passes (`src/render/*`), and interactive camera/HUD (`src/ui/*`, `src/app.*`).
- **Hydrodynamic boundary physics**: Velocity-damped CFL-safe solver (`dt = 0.5`, canonical row-major `y * width + x`) supporting four selectable boundary modes:
  - `ReflectiveNeumann` (rigid pool wall, in-phase reflection, ~2x wall run-up, volume conservation)
  - `PartialAbsorbRobin` (tunable wall reflection / gutter dissipation)
  - `AbsorbingOpen` (Mur absorbing boundary + sponge layer)
  - `FixedDirichlet` (legacy pinned zero border)
- **Floor + underwater wall caustics**: Exact ray-pool-box intersection projects refracted rays onto a continuous 5-face unfolded cross atlas (floor + South/East/North/West walls), with analytical Catmull-Rom derivatives, area-ratio flux, optional RGB chromatic dispersion, and analytical pool-rim sun shadows.
- **Water surface & dynamic waterline**: Live wave height traces the wet/dry waterline and meniscus along all four walls; water shading performs analytical underwater refraction with depth-validated screen sampling, Beer-Lambert depth absorption, capillary wall meniscus normals, and Fresnel reflections of both sky and dry pool walls.

## Interactive Controls

| Input | Action |
| --- | --- |
| Left-click on water | Add smooth Gaussian water drop |
| Right-drag | Orbit camera |
| Middle-drag or Shift + Right-drag | Pan camera |
| Mouse wheel | Zoom camera |
| `1` / `2` / `3` / `4` | Camera presets (Top-down, Classic oblique, Low corner wall-caustics view, Grazing side view) |
| `L` + Left-drag | Move light position across hemisphere |
| `Space` | Pause / resume simulation |
| `R` | Reset water surface |
| `B` | Cycle boundary mode (`NEUMANN` / `ROBIN` / `ABSORB` / `DIRICHLET`) |
| `H` | Toggle HUD overlay |
| `Esc` | Quit |

The top-left HUD provides live sliders for `AMP`, `SCALE`, `TEMPO`, `REFLECT`, `LIGHT EL`, `LIGHT AZ`, and `DISP`, plus a clickable Boundary Mode toggle.

## CLI Flags

```text
caustics [options]
  --dump <path.bmp> [frames]        Capture 24-bit BMP after N frames (default 180) and exit
  --wind <amp> <wavelength> <tempo> Set initial wind parameters
  --drop <gx> <gy> <amp> <frame>    Inject a drop at grid cell (gx, gy) on the given frame
  --camera <ex> <ey> <ez> <tx> <ty> <tz>  Set camera eye and target positions
  --preset <1|2|3|4>                Select camera preset
  --light <x> <y> <z>               Set light position
  --boundary <reflective|robin|absorb|dirichlet>  Select wave boundary condition
  --dispersion <val>                Set chromatic dispersion strength
  --no-ui                           Hide HUD overlay
```

## Build

Requirements: C++17 compiler, CMake 3.15+, OpenGL 3.3+, GLM, and GLFW 3.4+.

Initialize the GLM submodule first:

```bash
git submodule update --init external/glm
```

### Windows (MinGW-w64)

Download the [GLFW 3.4 64-bit Windows binaries](https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.bin.WIN64.zip) and copy `lib-mingw-w64` into `external/glfw-3.4.bin.WIN64/lib-mingw-w64`. Then build with `gcc`/`g++` on `PATH`:

```powershell
cmake -G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -S . -B build
cmake --build build
.\build\caustics.exe
```

### Linux / macOS / vcpkg

`CMakeLists.txt` automatically falls back to `find_package(glfw3)` and system GLFW/GLM installations when the bundled Windows MinGW libraries are not present:

```bash
# Example (Ubuntu/Debian): sudo apt install libglfw3-dev libglm-dev
# Example (macOS Homebrew): brew install glfw glm
cmake -S . -B build
cmake --build build
./build/caustics
```

## Verification Suite

Run all C++ verification tests (wave stability & 2^20 4x4 sweeps, boundary hydrodynamics, and pool coordinates/optics):

```powershell
powershell -ExecutionPolicy Bypass -File verify/check.ps1
```

Visual regression checks on captured 24-bit BMP dumps:

```powershell
# Floor caustic ring-band connectivity (classic oblique or top-down view)
.\build\caustics.exe --preset 2 --no-ui --dump .audit/classic.bmp 180
python verify/ring_band.py .audit/classic.bmp

.\build\caustics.exe --preset 1 --no-ui --dump .audit/topdown.bmp 180
python verify/ring_band.py --preset topdown .audit/topdown.bmp

# Submerged wall caustics, dry wall cleanliness, and water-wall seam check
.\build\caustics.exe --preset 3 --light 120 -120 140 --no-ui --dump .audit/walls.bmp 180
python verify/wall_caustics_check.py --preset 3 .audit/walls.bmp
```

See [.audit/caustic-contract.md](.audit/caustic-contract.md) for the full architectural and optical contract.
