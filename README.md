# Water caustics

The pool is a 400 by 400 wave grid. The floor is a checker of 25-unit tiles with grout. Four walls run from z = -30 up to z = 18. They shade as stone, except a thin band at the floor that shows caustics. AMP, SCALE, and TEMPO sliders drive the wind. A click outside the panel adds a drop of height 5. Escape closes the window.

The water mesh uses two vertices per simulation cell and stores world xy. Height and slope come from an R32F height texture, one float per cell. The slope is a Catmull-Rom sample at plus and minus 0.5 world units.

Caustics follow Snell's law. One light sits at (0, 0, 100). There is no dispersion. `shaders.h` holds the shaders. The caustic pass sends each water vertex to the pool floor along the refracted ray and writes how much surface lands on each floor pixel. A separable Gaussian blur with sigma 6 texels spreads each sample on the 2048 light map. The floor reads that map once and shows it on the tiles.

## Build

You need OpenGL 3.3, CMake 3.15, and MinGW-w64 g++ with C++17. This tree builds on Windows. Put `gcc` and `g++` on `PATH`.

GLM is a submodule at `external/glm`. The recorded commit is `2d4c4b4dd31fde06cfffad7915c2b3006402322f` from [g-truc/glm](https://github.com/g-truc/glm). A clone leaves that folder empty until you initialize it.

```
git submodule update --init external/glm
```

GLFW headers are in `external/glfw-3.4.bin.WIN64`. The MinGW libraries are not in git. `.gitignore` ignores `**/lib-*/`, `*.a`, and `*.dll`. Download the [GLFW 3.4 64-bit Windows binaries](https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.bin.WIN64.zip). Copy `lib-mingw-w64` from that zip into `external/glfw-3.4.bin.WIN64/`. CMake links `libglfw3.a` from that folder.

```
cmake -G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -S . -B build
cmake --build build
```

Run `build/caustics.exe`.

`verify/check.ps1` checks that the wave step stays finite and bounded. It links stub GLFW symbols and does not need `libglfw3.a`. `verify/ring_band.py` reads one dump. It prints the fraction of annulus pixels above the 90th percentile luminance that sit in the largest edge-connected piece. The annulus is world radius 94 to 120. From the repo root:

```
powershell -ExecutionPolicy Bypass -File verify/check.ps1
```
