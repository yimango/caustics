# Water caustics

The water grid is 400 by 400. Left click adds a wave. Escape closes the window.

Caustics are a refraction map. `shaders.h` holds the shaders. The caustic pass sends each water vertex to the pool floor along the refracted ray. The fragment writes how much surface lands on each floor pixel.

## Build

You need OpenGL 3.3, CMake 3.15, and a C++17 compiler. This tree is set up for Windows.

```
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

Run `caustics.exe`.

`verify/check.ps1` checks that the wave step stays finite and bounded.
