# Tiny2D Engine

A lightweight 2D game engine built with C++17.

## Project Status

Milestone M3 — Texture2D, SpriteRenderer and Camera2D implemented on top of M2.

- CMake static engine library and existing Vector2 math/tests
- RAII GLFW window and OpenGL Core context
- Logical/framebuffer size queries and resize-aware viewport
- Window-bound keyboard polling (WASD, Escape and GLFW key codes)
- Application update/render loop with monotonic, capped deltaTime
- Move-only RAII ShaderProgram, VertexBuffer and VertexArray
- GLSL 330 Core RGB triangle with interleaved position/color attributes
- External shader assets copied beside the executable; independent of working directory
- 800×600 Sandbox with an interpolated RGB triangle, WASD background and FPS title

## Requirements

- macOS Apple Silicon with AppleClang / Command Line Tools
- C++17 and CMake 3.20+
- GLFW 3.3+ and libpng (`brew install glfw libpng`)
- macOS system OpenGL (3.3 Core requested, forward-compatible)

CMake uses local dependencies only; it does not download third-party sources.

## Build, test and run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/tiny2d_sandbox --scene sprites
./build/tiny2d_sandbox --scene triangle --frames 4
./build/tiny2d_sandbox --scene sprites --sprite-count 500 --frames 120
# Also supported:
(cd build && ./tiny2d_sandbox)
# Deterministic normal-exit smoke run:
./build/tiny2d_sandbox --frames 4
```

If GLFW is not found, install it with Homebrew and add
`-DCMAKE_PREFIX_PATH="$(brew --prefix)"` to the configure command.

Hold W/A/D for green/red/blue channels; combine them to mix colors. S overrides
with dark gray. Release keys to restore the default background. Escape closes
the window. Resize freely; the title reports FPS approximately every second.

CTest includes Vector2Tests, deterministic rendering failure injection, real
OpenGL resource/pixel-readback tests, and Sandbox smoke runs from both directories.
The five `gui`/`opengl` tests need a logged-in macOS graphical session. They fail
on context errors rather than silently skipping. For a headless build explicitly
configure `-DTINY2D_OPENGL_TESTS=OFF`; that only validates CPU/fake-driver tests.

Debug build and all 5 tests passed on Apple M5 Pro (OpenGL 4.1 Core supplied for
the 3.3 Core request). The user confirmed the triangle, WASD, resizing and Escape.
See [M2 architecture, lifecycle and validation](docs/milestone-02.md) for exact
coverage and remaining manual checks, and [M1 design](docs/milestone-01.md) for the
preserved window/input/application foundation.

## Roadmap

- Textures, additional uniforms and indexed rendering
- Scene management
- Collision detection
- Arena Survivor demo

## M3

Default scene: 12 sprites sharing one CC0 PNG, center rotation, tint and overlapping
straight alpha. Arrow keys move the camera, Q/E zoom; movement and animation use
deltaTime. Draw Calls appear alongside FPS. `--sprite-count` accepts 1–500.
PNG assets use executable-relative paths; no working-directory dependency.

Debug and Release: 8/8 CTest passed in a real macOS graphical session. Manual
keyboard/resize observation remains pending; see the explicit validation limits.

- [Architecture, math, lifecycle and dependency licensing](docs/milestone-03.md)
- [Measured performance baseline and M4 direction](docs/performance-baseline.md)
- [Review notes and actual acceptance coverage](docs/m3-review-notes.md)
