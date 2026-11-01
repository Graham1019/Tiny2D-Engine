# v0.4.0 — candidate release notes

Status: prepared for integration review, not released. Tag and GitHub Release
require user confirmation after the integration PR merges and main is validated.
Implementation baseline: `cee42f3e34693d2777d5f4545c697af62029b67d` (M1–M4.1).

## Added

- GLFW Window, Input, Application loop, deltaTime and framebuffer/Retina size handling.
- ShaderProgram, VBO/VAO, external GLSL triangle assets and compile/link diagnostics.
- libpng ImageLoader, RGBA8 Texture2D, Mat4, orthographic Camera2D and SpriteRenderer.
- Ordered multi-texture BatchRenderer, dynamic vertex buffer, fixed index buffer,
  automatic capacity/slot flush, RenderStats and CPU/GPU Benchmark.

## Improved

GPU RAII cleanup and failure diagnostics; owner-context validation before batch
queue changes; safe batch move assignment; selected renderer initialization in
Sandbox; straight-alpha composition and constant-index GLSL 330 texture sampling.
Borrowed textures must still remain alive until flush.

## Tested

2026-11-01 on Apple M5 Pro, macOS 26.5.1, AppleClang 21, OpenGL 4.1 Metal (3.3 Core
requested): Debug and Release builds, 10/10 CTest each, including seven real-driver
tests. Dual unshared contexts, moved-from and assignment cases, allocation failure
cleanup, batch capacity, alpha order and cross-slot RGBA regression passed; maximum
pixel error 0 in both orders. Six 30-frame Sandbox scenarios passed. Default
Benchmark recorded 9600 CPU and 9600 GPU samples in a new directory.

## Performance

500-sprite CPU workload historical M4.1: Immediate 500 draws / 489.92 µs CPU p50;
Batch 1 draw / 90.67 µs (~5.40× submission ratio, 99.8% fewer calls).
Integration reproduction: 500/1 draws and 495.75/87.83 µs (~5.64×). 5000-sprite
multitexture Sandbox: 5 draws; zero sprites: 0 draws for either renderer.
These are draw-call and CPU submission measurements, not FPS claims. Pixel-load
latencies vary substantially across sessions; no blanket performance guarantee.
See [M4.1 report](performance-m41.md) and [integration evidence](integration-m1-m4.md).
Historical raw data remains unchanged.

## Limitations

macOS-only, system OpenGL, no cross-driver validation. GPU operations and destruction
require the main thread and live owning context. Queued texture lifetime is the
caller's responsibility. Manual input/window/Retina-transition acceptance, GPU memory
soak and repeated-session latency validation remain pending. No automated GitHub CI
workflow or repository-wide engine license is supplied by this candidate; existing
texture and dependency copyright notices are retained.

## Next

After approved integration into main, create `feat/scene-collision` from updated
`origin/main`. M5: Scene Management, GameObject/Transform2D, Collision2D, Fixed
Timestep, Texture Lifetime and Interactive Arena Prototype. None is implemented
in this release-preparation change.
