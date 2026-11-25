# Caustic interface contract

Reference for `main.cpp` and `shaders.h`. The pool, the wave step, and this map are the whole feature.

## Decision

The caustic pass projects each water vertex to the floor along the refracted ray, then writes flux from how much water surface lands on each floor pixel. `shaders.h` owns that pair of shaders. `main.cpp` owns the ortho camera, the additive framebuffer, and the uploads.

The previous fragment shader kept `gl_Position` on the water surface and stored `length(T.xy)`. That paints a soft blob under the wave. A vertex bend without the area ratio was reverted because it looked worse than the area-ratio rings.

Rejected alternative. Write `1/|det d(hit)/d(x,y)|` into the texel under the surface point. The energy is then still in the wrong place.

## Locked scene

| Name | Value |
| --- | --- |
| Grid `width`, `height` | 400, 400 |
| `waterScale` | 1 |
| Pool half-extent | `(width / 2) * waterScale` = 200 |
| Pool XY | [-200, 200] |
| `BOTTOM_Z` | -30 |
| `WATER_IOR`, `AIR_IOR` | 1.33, 1.0 |
| `CAUSTICS_RES` | 2048 |
| Light position | (0, 0, 100) |
| Mesh height | `height_current * 8` |
| Mesh slope | Catmull-Rom sample of `heightTex` at plus and minus 0.5 world units; the visible normal multiplies that slope by 8 |

Do not change `dx`, `dt`, `c`, `damping`, `width`, or `height`. Do not remove the height scale of 8.

Wave step, unchanged:

```
laplacian = E + W + N + S - 4 * C
height_next = (1 - damping) * (2 * C - P) + (c * c * dt * dt / (dx * dx)) * laplacian
```

The loop writes the interior only, cells `1 .. n-2`. The border of `height_next` stays 0. Then copy `height_prev = height_current` and `height_current = height_next`. Do not swap. A swap keeps stale border cells.

## Height storage

Flat row-major `std::vector<float>`. Index is `x * height + y`.

```
std::vector<float> height_current;
std::vector<float> height_prev;
std::vector<float> height_next;
```

`init_grid` resizes all three to `width * height` and sets every cell to 0. `add_disturbance` keeps the same border rejection and writes one cell. `verify/wave_stability.cpp` uses this layout. The scenarios, ceilings, and linearity test stay as they are.

## Mesh

`buildWaterMesh` runs once. The simulation index stays `x * height + y`. The drawn mesh uses `meshSubdiv` 2, so each axis has `(n - 1) * 2 + 1` vertices. Vertex id on that mesh is `i * axisY + j`. The buffer stores world xy and z = 0. Two triangles per fine cell. Both buffers are `GL_STATIC_DRAW`. The shader samples height and slope from `heightTex`.

## Framebuffer and UV

`setupCausticsFBO` creates two `GL_RGBA32F` textures at `CAUSTICS_RES`, with a linear filter and clamp-to-edge. `causticsFBO` holds the light map. `causticsBlurFBO` holds the horizontal blur. Color attachment 0 is cleared to 0. The light map is cleared again every frame before the mesh draw.

Caustic camera, every frame:

```
extent = (width / 2) * waterScale
view = lookAt((0, 0, 80), (0, 0, 0), (0, 1, 0))
projection = ortho(-extent, extent, -extent, extent, 0.1, 300)
```

Draw the water mesh into `causticsFBO` with depth off and `glBlendFunc(GL_ONE, GL_ONE)`. `blurCausticMap` then turns blending off and draws a fullscreen triangle twice. The horizontal pass reads `causticsTexture` and writes `causticsBlurTexture`. The vertical pass reads `causticsBlurTexture` and writes `causticsTexture`. Each pass is a normalized Gaussian with sigma 6 texels and offsets from -18 to 18. `direction` is `(1/CAUSTICS_RES, 0)`, then `(0, 1/CAUSTICS_RES)`. The shader forms the sample coordinate as `gl_FragCoord.xy` times the texel size in `direction`. The render loop then restores the window viewport.

Floor sample:

```
causticsUV = (FragPos.xy + vec2(poolExtent)) / (poolExtent * 2.0)
light = texture(causticsTexture, causticsUV).r
```

UV (0, 0) is world (-200, -200). UV (1, 1) is world (200, 200). The ortho projection uses the same square, so a floor hit and a floor sample name the same texel.

## Who owns which shader

| Program | Vertex | Fragment |
| --- | --- | --- |
| Water | `surfaceVertexShaderSource` | `waterFragmentShaderSource` |
| Floor | `bottomVertexShaderSource` | `bottomFragmentShaderSource` |
| Caustics | `causticVertexShaderSource` | `causticsFragmentShaderSource` |
| Caustic blur | `causticBlurVertexShaderSource` | `causticBlurFragmentShaderSource` |
| Sky | `skyboxVertexShaderSource` | `skyboxFragmentShaderSource` |
| Overlay | `overlayVertexShaderSource` | `overlayFragmentShaderSource` |

`surfaceVertexShaderSource` samples height with Catmull-Rom and outputs `FragPos` and `Normal`. The slope is that curve at plus and minus 0.5 world units. Water uses it. The floor uses `bottomVertexShaderSource`, which is only the model-view-projection transform. The caustic pass uses `causticVertexShaderSource`.

`main` compiles the caustic program from `causticVertexShaderSource` and `causticsFragmentShaderSource`. It compiles the blur program from `causticBlurVertexShaderSource` and `causticBlurFragmentShaderSource`. The floor uses `bottomVertexShaderSource` and `bottomFragmentShaderSource`. The floor fetches the blurred map once.

## Caustic uniforms

`main` uploads these on the caustic program. The caustic shaders read them.

| Uniform | Type | Value |
| --- | --- | --- |
| `model` | mat4 | identity |
| `view` | mat4 | caustic `lookAt` above |
| `projection` | mat4 | caustic `ortho` above |
| `lightPos` | vec3 | (0, 0, 100) |
| `bottomZ` | float | `BOTTOM_Z` |
| `waterIOR` | float | 1.33 |
| `airIOR` | float | 1.0 |
| `heightScale` | float | `meshHeightScale` (8) |
| `poolExtent` | float | `(width / 2) * waterScale` |
| `meshSpacing` | float | world distance between drawn water vertices |

`main` uploads `heightScale`, `poolExtent`, `gridSize`, and `heightTex` on texture unit 1. `poolExtent` is `(width / 2) * waterScale`. The height texture is R32F, one float per cell, updated with `glTexSubImage2D` after each wave step. There is no geometry shader, no `time` uniform, and no `receiverTexel` uniform.

The blur program reads `source` on texture unit 0 and `direction`. The horizontal pass sets `direction` to `(1/CAUSTICS_RES, 0)`. The vertical pass sets `direction` to `(0, 1/CAUSTICS_RES)`.

Beauty pass uploads:

- Floor reads `model`, `view`, `projection`, `causticsTexture` on texture unit 0, `poolExtent`, and `bottomZ`.
- Water reads `model`, `view`, `projection`, `lightPos`, `viewPos`, `heightScale`, `poolExtent`, `gridSize`, and `heightTex` on texture unit 1.
- Sky reads `view` with translation removed, and `projection`.

Camera for the beauty pass stays `lookAt((90, -120, 95), (0, 0, -10), (0, 0, 1))`, perspective 50 degrees, aspect `SCR_WIDTH / SCR_HEIGHT`, near 0.1, far 800.

## Caustic vertex shader

The mesh stores world xy. The shader samples `heightTex` with Catmull-Rom. The optical normal is `normalize(vec3(-dhdx, -dhdy, 1))` from that curve at plus and minus 0.5 world units. The visible water normal multiplies that slope by `heightScale`. The Jacobian step is `meshSpacing`.

The ray starts at the simulated height, not the exaggerated mesh height. `gl_Position` is the floor hit. Intensity is a smooth varying.

```
receiver = abs(cross(hit(x+dx) - hit, hit(y+dy) - hit))
intensity = clamp(dx * dy / max(receiver, 0.0001), 0.0, 4.0)
```

On flat water, intensity is about 1. Where neighboring rays land closer together, intensity rises. The Gaussian blur then spreads each sample by sigma 6 texels, about 1.17 world units on the 400-unit pool. The Snell hit and the area ratio stay in the vertex shader.

## Caustic fragment shader

```
FragColor = vec4(intensity)
```

## Floor and water

Floor tiles are 25 world units. Tile A is `vec3(0.62, 0.70, 0.66)`. Tile B is `vec3(0.33, 0.50, 0.52)`. Grout is `vec3(0.16, 0.22, 0.24)`, about 0.06 of the tile. The light-map sample is tonemapped before it multiplies the tile.

```
tone = light / (1.0 + 0.85 * max(light - 0.65, 0.0))
color = albedo * (0.42 + 1.15 * tone)
FragColor = vec4(clamp(color, 0.0, 1.0), 1.0)
```

Four walls share the floor mesh and the floor program. Wall z runs from `BOTTOM_Z` to 18. When `FragPos.z` is above `bottomZ + 0.5`, the fragment shades a stone band and does not sample caustics. The sky gradient is still drawn first, with depth off, as the background. This GLAD has no `glDepthFunc`.

Water stays a Fresnel mix of `vec3(0.03, 0.28, 0.36)` and `vec3(0.40, 0.66, 0.78)`, specular power 48, alpha from 0.28 to 0.52. The floor must stay visible through it.

## Click

A left click outside the slider panel unprojects the cursor with the beauty camera and the framebuffer size, hits the plane `z = 0`, then

```
gridX = hit.x / waterScale + width / 2
gridY = hit.y / waterScale + height / 2
```

`add_disturbance(gridX, gridY, 5.0)` when the indices are inside the grid. A click on the panel drags a slider and does not add a drop. Escape still closes the window.

## Dump

`main` accepts `caustics.exe --dump <path.bmp> <frames>`. After that many frames, read the window framebuffer, write a 24-bit BMP, and exit. Default frames if the count is missing: 180. This is how the proof frame is captured.

## Comments

`shaders.h` and `main.cpp` contain no comments. This file holds the invariants.
