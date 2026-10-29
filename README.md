# Tiny2D Engine

A lightweight 2D game engine built with C++17.

## Project Status

Milestone M1 — Window + Input + Game Loop implemented.

- CMake static engine library and existing Vector2 math/tests
- RAII GLFW window and OpenGL Core context
- Logical/framebuffer size queries and resize-aware viewport
- Window-bound keyboard polling (WASD, Escape and GLFW key codes)
- Application update/render loop with monotonic, capped deltaTime
- 800×600 graphical Sandbox with key-controlled colors and FPS title

## Requirements

- macOS Apple Silicon with AppleClang / Command Line Tools
- C++17 and CMake 3.20+
- GLFW 3.3+ (`brew install glfw`)
- macOS system OpenGL (3.3 Core requested, forward-compatible)

CMake uses local dependencies only; it does not download third-party sources.

## Build, test and run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/tiny2d_sandbox
```

If GLFW is not found, install it with Homebrew and add
`-DCMAKE_PREFIX_PATH="$(brew --prefix)"` to the configure command.

Hold W/A/D for green/red/blue channels; combine them to mix colors. S overrides
with dark gray. Release keys to restore the default background. Escape closes
the window. Resize freely; the title reports FPS approximately every second.

See [M1 design and acceptance guide](docs/milestone-01.md) for ownership, loop
flow, build details, manual checks and known limitations. Build and Vector2Tests
passed; the user confirmed window display, WASD/mixed-color behavior, Escape exit and
FPS updates. Other GUI acceptance items are documented as unverified.

## Roadmap

- Modern OpenGL renderer, shaders and textures
- Scene management
- Collision detection
- Arena Survivor demo
