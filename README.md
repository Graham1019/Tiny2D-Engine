# Tiny2D Engine

## Project Overview

Tiny2D is a C++17 learning engine for macOS OpenGL. The v0.4.0 candidate integrates
M1–M4.1: window/input, programmable rendering, textured sprites, a 2D camera and
order-preserving batch rendering. This is release preparation, not a published release.

## Features

- GLFW Window, Input and Application loop with deltaTime and framebuffer resize support.
- Move-only GPU ownership, shader compile/link diagnostics, external triangle shaders, VBO/VAO.
- libpng RGBA8 decoding, Texture2D, Mat4, orthographic Camera2D and alpha-blended SpriteRenderer.
- BatchRenderer with dynamic VBO, fixed EBO, up to eight texture slots, automatic flush and RenderStats.
- GLSL 330 constant-index sampling, transparent submission order, CPU/GPU benchmark and regression tests.
- M4.1 context validation, safe batch move assignment and full-frame cross-slot pixel comparisons.

## Architecture

Application owns Window and Input. Sandbox GPU members are destroyed before the
base Application's window/context. ImageLoader produces CPU pixels; Texture2D
owns uploaded GPU storage. Camera2D supplies the view-projection matrix to either
SpriteRenderer or BatchRenderer. BatchRenderer preserves submission order and
restores its documented GL state at endFrame.

See the [batch state and lifetime contract](docs/batch-rendering-design.md).

## Requirements

macOS (validated on Apple Silicon), AppleClang, C++17, CMake 3.20+, GLFW 3.3+,
system OpenGL with a 3.3 Core context, and libpng. Real GPU tests require a graphical
login. Dependencies must be installed locally; configuration does not fetch them.

```sh
brew install glfw libpng
```

## Build Instructions

```sh
cmake -S . -B build/integration-debug -DCMAKE_BUILD_TYPE=Debug -DTINY2D_OPENGL_TESTS=ON
cmake --build build/integration-debug --parallel
cmake -S . -B build/integration-release -DCMAKE_BUILD_TYPE=Release -DTINY2D_OPENGL_TESTS=ON
cmake --build build/integration-release --parallel
```

If discovery fails, add `-DCMAKE_PREFIX_PATH="$(brew --prefix)"` to configuration.
Builds generate the static engine library, Sandbox, Benchmark and test executables.
Assets are copied beside Sandbox on each build and resolve independently of the working directory.

## Run Instructions

```sh
./build/integration-release/tiny2d_sandbox --scene triangle --frames 30
./build/integration-release/tiny2d_sandbox --renderer immediate --scene sprites --sprite-count 500 --frames 30
./build/integration-release/tiny2d_sandbox --renderer batch --scene sprites --sprite-count 500 --frames 30
./build/integration-release/tiny2d_sandbox --renderer batch --scene multitexture --sprite-count 5000 --frames 30
```

Omit `--frames` for interaction. Arrow keys pan, Q/E zoom, WASD change background,
Escape exits. Default renderer is immediate; sprite count accepts 0–100000.
Interactive VSync is enabled; Benchmark disables it.

## Test Instructions

```sh
ctest --test-dir build/integration-debug -N
ctest --test-dir build/integration-debug --output-on-failure
ctest --test-dir build/integration-release -N
ctest --test-dir build/integration-release --output-on-failure
```

Integration validation on 2026-11-01 passed 10/10 in both configurations, including
seven real OpenGL tests. Restricted process execution could not complete GPU tests;
rerunning in the logged-in desktop environment passed. For CPU/fake-driver checks
only, configure a separate build with `-DTINY2D_OPENGL_TESTS=OFF`; that does not
validate actual rendering. See [integration evidence and manual-test gaps](docs/integration-m1-m4.md).

## Performance

500 identical-texture sprites produce 500 Immediate draws versus 1 Batch draw
(99.8% fewer calls). Historical M4.1 CPU submission p50 is 489.92 µs versus
90.67 µs (about 5.40×); that ratio is not an FPS improvement. GPU timings and
pixel workloads vary, and some workloads regress.

- [M4 performance](docs/performance-m4.md) and [M4.1 performance](docs/performance-m41.md)
- [Integration reproduction](docs/integration-m1-m4.md) and [new raw data](docs/data/integration-m1-m4/)

Run Benchmark in a **new empty directory**: it writes `m4-frames.csv` and
`m4-gpu-frames.csv` in the working directory. Never overwrite historical data.

```sh
mkdir -p build/benchmark-new-run
(cd build/benchmark-new-run && ../integration-release/tiny2d_benchmark > trials.csv 2> environment.txt)
python3 scripts/summarize_m4.py build/benchmark-new-run/trials.csv
```

Default measurement uses 30 warmup frames, 120 samples, five repeats, two workloads
and 100/500/1000/5000 sprites. CPU submission and asynchronous GPU timer intervals
are reported separately.

## Project Structure

| Path | Purpose |
|---|---|
| engine/include, engine/src | Engine API and implementation |
| sandbox/src, sandbox/assets | Scenes, Benchmark, shaders and original textures |
| tests | CPU, injected-failure and real-driver regression tests |
| scripts | Benchmark aggregation |
| docs | Milestones, design, review notes and immutable historical measurements |

Milestones: [M1](docs/milestone-01.md), [M2](docs/milestone-02.md),
[M3](docs/milestone-03.md), [M4](docs/milestone-04.md),
[M4.1 review fixes](docs/m4-review-notes.md). Historical documents describe their
stage; current batch behavior is specified in the batch design document.

## Roadmap

M1–M4.1 are implemented. After integration review and merge, M5 will start from
updated main: Scene Management, GameObject/Transform2D, Collision2D, Fixed Timestep,
Texture Lifetime and an Interactive Arena Prototype. M5 and M6 are not completed.
See [v0.4.0 candidate release notes](docs/release-notes-v0.4.0.md).

## Known Limitations

macOS-only build; no cross-platform or cross-driver support claim. OpenGL is Apple's
system API and no alternative graphics backend is implemented. GPU work and destruction
require the owning live context on the main thread; destruction misuse terminates.
Queued textures must survive until flush. No resource cache, scene system, collision,
sRGB pipeline or multithreaded OpenGL is provided. Manual input/window interaction,
cross-display Retina transitions and long-duration GPU memory profiling remain pending.
No GitHub CI workflow is currently configured; validation here is local.

Original texture copyright/CC0 notices are preserved in
[LICENSE.txt](sandbox/assets/textures/LICENSE.txt) and
[M4-LICENSE.txt](sandbox/assets/textures/M4-LICENSE.txt). libpng attribution is in
[M3 documentation](docs/milestone-03.md). No repository-wide LICENSE file exists;
this integration does not assign a new license to the engine code.
