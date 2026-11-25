# Caustic interface contract

Reference for `main.cpp` and `shaders.h`. The pool, the wave step, and this map are the whole feature.

## Decision

The caustic pass projects each water vertex to the floor along the refracted ray, then writes flux from how much water surface lands on each floor pixel. `shaders.h` owns that pair of shaders. `main.cpp` owns the ortho camera, the additive framebuffer, and the uploads.

The previous fragment shader kept `gl_Position` on the water surface and stored `length(T.xy)`. That paints a soft blob under the wave. `frame-curvature.png` is that result. A vertex bend without the area ratio was reverted because `frame-final.png` looked worse than `frame-400.png`.

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
| Mesh slope | `getSurfaceNormal` already multiplies the height difference by 8 |

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

`buildWaterIndices` runs once. Vertex id is `x * height + y`, the same index as the height arrays. Two triangles per cell, same winding as today.

`fillWaterVertices` runs every frame. It writes position and normal only, then `glBufferSubData` on the vertex buffer. The element buffer is `GL_STATIC_DRAW` once.

## Framebuffer and UV

`setupCausticsFBO` is unchanged. `GL_RGBA32F`, `CAUSTICS_RES` square, linear filter, color attachment 0, cleared to 0.

Caustic camera, every frame:

```
extent = (width / 2) * waterScale
view = lookAt((0, 0, 80), (0, 0, 0), (0, 1, 0))
projection = ortho(-extent, extent, -extent, extent, 0.1, 300)
```

Draw the water mesh into that framebuffer with depth off and `glBlendFunc(GL_ONE, GL_ONE)`. Then restore the window viewport.

Floor sample, unchanged:

```
causticsUV = (FragPos.xy + vec2(200.0)) / 400.0
causticIntensity = texture(causticsTexture, causticsUV).a
```

UV (0, 0) is world (-200, -200). UV (1, 1) is world (200, 200). The ortho projection uses the same square, so a floor hit and a floor sample name the same texel.

## Who owns which shader

| Program | Vertex | Fragment |
| --- | --- | --- |
| Water | `surfaceVertexShaderSource` | `waterFragmentShaderSource` |
| Floor | `surfaceVertexShaderSource` | `bottomFragmentShaderSource` |
| Caustics | `causticVertexShaderSource` | `causticsFragmentShaderSource` |
| Sky | `skyboxVertexShaderSource` | `skyboxFragmentShaderSource` |

`surfaceVertexShaderSource` is only the model-view-projection transform. It outputs `FragPos` and `Normal`. Water and the floor use it. The caustic pass does not.

`main` compiles the caustic program from `causticVertexShaderSource` and `causticsFragmentShaderSource`.

## Caustic uniforms

`main` uploads these on the caustic program. The caustic shaders read them. No other caustic uniform exists.

| Uniform | Type | Value |
| --- | --- | --- |
| `model` | mat4 | identity |
| `view` | mat4 | caustic `lookAt` above |
| `projection` | mat4 | caustic `ortho` above |
| `lightPos` | vec3 | (0, 0, 100) |
| `bottomZ` | float | `BOTTOM_Z` |
| `waterIOR` | float | 1.33 |
| `airIOR` | float | 1.0 |
| `receiverTexel` | float | `(width * waterScale) / CAUSTICS_RES` |

`receiverTexel` is the world width of one framebuffer pixel, 400/2048. There is no `time` uniform.

Beauty pass uploads, unchanged except dropping `time`:

- Floor reads `model`, `view`, `projection`, `causticsTexture` on texture unit 0.
- Water reads `model`, `view`, `projection`, `lightPos`, `viewPos`.
- Sky reads `view` with translation removed, and `projection`.

Camera for the beauty pass stays `lookAt((90, -120, 95), (0, 0, -10), (0, 0, 1))`, perspective 50 degrees, aspect `SCR_WIDTH / SCR_HEIGHT`, near 0.1, far 800.

## Caustic vertex shader

Inputs are `aPos` and `aNormal`. Outputs are `FragPos` (surface position in world space), `Normal`, and `float casts`.

1. `P = (model * vec4(aPos, 1)).xyz`
2. `N = normalize(normalMatrix * aNormal)`. If `N.z < 0`, negate `N`.
3. `I = normalize(P - lightPos)`
4. `eta = airIOR / max(waterIOR, 0.001)`
5. `T = refract(I, N, eta)`
6. If `T.z < -0.02`, `t = (bottomZ - P.z) / T.z`. If `t > 0`, `hit = P + T * t` and `casts = 1`.
7. If `casts < 0.5`, set `gl_Position` to `vec4(2, 2, 2, 1)` so the triangle is clipped.
8. Otherwise `gl_Position = projection * view * vec4(hit, 1)`.

`FragPos` stays `P`. Do not write the hit into `FragPos`.

## Caustic fragment shader

If `casts < 0.5`, discard.

```
area = abs(dFdx(FragPos.x) * dFdy(FragPos.y) - dFdx(FragPos.y) * dFdy(FragPos.x))
intensity = clamp(area / max(receiverTexel * receiverTexel, 1e-8), 0.0, 6.0)
FragColor = vec4(intensity)
```

On flat water under this light, `intensity` is about 1. Where the surface squeezes onto fewer floor pixels, `intensity` rises. The cap is 6. The floor reads alpha, which matches the red channel.

## Floor and water

Floor, so filaments read on a dark pool:

```
light = causticIntensity
floorColor = vec3(0.05, 0.14, 0.20)
lit = floorColor * (0.25 + light)
hot = vec3(1.45, 1.15, 0.62) * max(light - 1.0, 0.0)
FragColor = vec4(clamp(lit + hot, 0.0, 1.0), 1.0)
```

Water stays a Fresnel mix of `vec3(0.03, 0.28, 0.36)` and `vec3(0.40, 0.66, 0.78)`, specular power 48, alpha from 0.28 to 0.52. The floor must stay visible through it. Sky stays the two-stop gradient and is drawn first, with depth off. This GLAD has no `glDepthFunc`.

Drop `time` from every shader that still declares it.

## Click

A left click unprojects the cursor with the beauty camera and the framebuffer size, hits the plane `z = 0`, then

```
gridX = hit.x / waterScale + width / 2
gridY = hit.y / waterScale + height / 2
```

`add_disturbance(gridX, gridY, 5.0)` when the indices are inside the grid. Escape still closes the window.

## Dump

`main` accepts `caustics.exe --dump <path.bmp> <frames>`. After that many frames, read the window framebuffer, write a 24-bit BMP, and exit. Default frames if the count is missing: 180. This is how the proof frame is captured.

## Comments

`shaders.h` and `main.cpp` contain no comments. This file holds the invariants.
