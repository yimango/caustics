# Water caustics

The pool is a 400 by 400 wave grid. The floor is a checker of 25-unit tiles with grout. Four walls run from the floor at z = -30 up to z = 18 and shade as stone above the water. AMP, SCALE, and TEMPO sliders drive the wind. A click outside the panel adds a drop of height 5. Escape closes the window.

The water mesh has one vertex per cell and stores world xy. Height and slope come from an R32F height texture, one float per cell. The slope is a one-texel central difference, so the drawn ripples follow the grid.

Caustics follow Snell's law. `shaders.h` holds the shaders. The caustic pass sends each water vertex to the pool floor along the refracted ray and writes how much surface lands on each floor pixel. The floor reads that map through a 5-tap cross.

## Build

You need OpenGL 3.3, CMake 3.15, and a C++17 compiler. This tree is set up for Windows. GLFW headers are under `external/glfw-3.4.bin.WIN64`. The import library in `lib-mingw-w64` is not in git. GLM is the `external/glm` pin.

```
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

From `build`, run `caustics.exe`.

`verify/check.ps1` checks that the wave step stays finite and bounded.
