> Historical README snapshot before M6.3c; versions, commands and pending items below describe that period.

# Tiny2D Engine

## Project Overview

Tiny2D is a C++17 learning engine for macOS OpenGL. M0–M6.2 include window/input, batched sprites, Scene/collision, fixed-step combat,
and a playable 90-second survivor game with HUD, victory/defeat and restart.
The project version remains 0.4.0; no release is published.

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

See the [batch state and lifetime contract](batch-rendering-design.md).

## Requirements

macOS (validated on Apple Silicon), AppleClang, C++17, CMake 3.20+, GLFW 3.3+,
system OpenGL with a 3.3 Core context, and libpng. Real GPU tests require a graphical
login. Dependencies must be installed locally; configuration does not fetch them.

```sh
brew install glfw libpng
```

## Build Instructions

```sh
cmake -S . -B build/m62-debug -DCMAKE_BUILD_TYPE=Debug -DTINY2D_OPENGL_TESTS=ON
cmake --build build/m62-debug --parallel
cmake -S . -B build/m62-release -DCMAKE_BUILD_TYPE=Release -DTINY2D_OPENGL_TESTS=ON
cmake --build build/m62-release --parallel
```

If discovery fails, add `-DCMAKE_PREFIX_PATH="$(brew --prefix)"` to configuration.
Builds generate the static engine library, Sandbox, Benchmark and test executables.
Assets are copied beside Sandbox on each build and resolve independently of the working directory.

## Run Instructions

```sh
./build/m62-release/tiny2d_sandbox --scene triangle --frames 30
./build/m62-release/tiny2d_sandbox --renderer immediate --scene sprites --sprite-count 500 --frames 30
./build/m62-release/tiny2d_sandbox --renderer batch --scene sprites --sprite-count 500 --frames 30
./build/m62-release/tiny2d_sandbox --renderer batch --scene multitexture --sprite-count 5000 --frames 30
```

Omit `--frames` for interaction. Arrow keys pan, Q/E zoom, WASD change background,
Escape exits. Default renderer is immediate; sprite count accepts 0–100000.
Interactive VSync is enabled; Benchmark disables it.

## Test Instructions

```sh
ctest --test-dir build/m62-debug -N
ctest --test-dir build/m62-debug --output-on-failure
ctest --test-dir build/m62-release -N
ctest --test-dir build/m62-release --output-on-failure
```

Historical integration validation on 2026-11-01 passed 10/10 in both configurations, including
seven real OpenGL tests. Restricted process execution could not complete GPU tests;
rerunning in the logged-in desktop environment passed. For CPU/fake-driver checks
only, configure a separate build with `-DTINY2D_OPENGL_TESTS=OFF`; that does not
validate actual rendering. See [integration evidence and manual-test gaps](integration-m1-m4.md).

## Performance

500 identical-texture sprites produce 500 Immediate draws versus 1 Batch draw
(99.8% fewer calls). Historical M4.1 CPU submission p50 is 489.92 µs versus
90.67 µs (about 5.40×); that ratio is not an FPS improvement. GPU timings and
pixel workloads vary, and some workloads regress.

- [M4 performance](performance-m4.md) and [M4.1 performance](performance-m41.md)
- [Integration reproduction](integration-m1-m4.md) and [new raw data](data/integration-m1-m4/)

Run Benchmark in a **new empty directory**: it writes `m4-frames.csv` and
`m4-gpu-frames.csv` in the working directory. Never overwrite historical data.

```sh
mkdir -p build/benchmark-new-run
(cd build/benchmark-new-run && ../m62-release/tiny2d_benchmark > trials.csv 2> environment.txt)
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

Milestones: [M1](milestone-01.md), [M2](milestone-02.md),
[M3](milestone-03.md), [M4](milestone-04.md),
[M4.1 review fixes](m4-review-notes.md). Historical documents describe their
stage; current batch behavior is specified in the batch design document.

## Roadmap

M0–M6.2 are implemented. M6.3 will consider visual polish, measured performance
optimization, extended playtesting and release packaging.

## Known Limitations

macOS-only build; no cross-platform or cross-driver support claim. OpenGL is Apple's
system API and no alternative graphics backend is implemented. GPU work and destruction
require the owning live context on the main thread; destruction misuse terminates.
Queued textures must survive until flush. No sRGB pipeline or multithreaded OpenGL is provided. Manual input/window interaction,
cross-display Retina transitions and long-duration GPU memory profiling remain pending.
No GitHub CI workflow is currently configured; validation here is local.

Original texture copyright/CC0 notices are preserved in
[LICENSE.txt](../sandbox/assets/textures/LICENSE.txt) and
[M4-LICENSE.txt](../sandbox/assets/textures/M4-LICENSE.txt). libpng attribution is in
[M3 documentation](milestone-03.md). No repository-wide LICENSE file exists;
this integration does not assign a new license to the engine code.

## M5 Development: Scene / Collision / Arena

M5 adds CPU-only generation handles, deterministic Scene iteration, closed-set
AABB/Circle collision events, and a bounded 60 Hz accumulator. SceneRenderer
bridges numeric sprite asset keys to shared textures and retains ownership until
BatchRenderer finishes its pass. Existing M0–M4.1 scenes remain available.

```sh
cmake -S . -B build/m5-debug -DCMAKE_BUILD_TYPE=Debug -DTINY2D_OPENGL_TESTS=ON
cmake --build build/m5-debug --parallel
ctest --test-dir build/m5-debug --output-on-failure
./build/m5-debug/tiny2d_sandbox --scene arena-demo --frames 120
./build/m5-debug/tiny2d_sandbox --scene arena-demo
```

WASD moves the player, arrows offset the follow camera, Q/E zoom, Escape exits.
Arena always uses batching; renderer/sprite-count options retain their existing
meaning for the old sprite demos. Finite Arena runs advance one fixed tick per
render frame for reproducible lifecycle smoke coverage. Interactive runs use real
time. This is a prototype, not a complete Arena Survivor game.

See [M5 acceptance and limitations](milestone-05.md),
[scene ownership](scene-architecture.md), [collision](collision-design.md),
[fixed timestep](fixed-timestep.md), and [review notes](m5-review-notes.md).

M5.1 review fixes add enforced Scene identity, full-scene render preflight,
pending-command cancellation and per-texture frame ownership. Predictable Scene
input failures draw nothing; cancel cannot undo completed GPU draws. See
[M5.1 contracts, tests and performance](m51-review-fixes.md).

### M6.2 Arena Survivor

```bash
./build/m62-release/tiny2d_sandbox --scene survivor-demo
./build/m62-release/tiny2d_sandbox --scene survivor-demo --frames 600
```

WASD moves, arrows offset camera, Q/E zoom, Escape exits. Survive 5400 fixed ticks
(90 simulation seconds) to win. HP zero ends the game; death wins same-tick ties.
World simulation freezes at either outcome, while the HUD and result panel remain
visible. Press and release R to restart. HP/bar, countdown and kills are rendered
inside the window. Finite runs keep rendering to their limit after death.

See [M6.2 validation](milestone-06-2.md), [game loop](game-loop-design.md),
[HUD](hud-rendering.md), and historical [M6.1](milestone-06-1.md).

## M6.3a difficulty and menu

Survivor now opens a CPU-only difficulty-selection flow (default Lv.3); 1–5 selects,
Enter starts, and terminal R restarts the applied level. Direct GameSession defaults
remain Lv.1/M6.2. HUD shows the applied level at lower left using the existing UI pass.
Finite --frames runs auto-start; --difficulty accepts 1..5.
See [M6.3a validation and commands](milestone-06-3a.md), [difficulty design](difficulty-design.md)
and [menu lifecycle](menu-flow-design.md).

## M6.3b combat presentation

Bounded combat events feed CPU-only hit/death sparks, trails, center marks and damage
feedback. World camera shake leaves UI independent. Difficulty rules are unchanged.
FPS uses a recent one-second window, including menu rendering.
See [milestone](milestone-06-3b.md), [feedback](combat-feedback-design.md),
[lifecycle](vfx-lifecycle.md) and [performance](performance-m63b.md).
