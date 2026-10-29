
# Tiny2D Engine

A lightweight 2D game engine built with C++17.

## Project Status

Day 1 — Initial development.

Implemented:
- CMake build system
- Engine static library
- Vector2 constructors
- Vector2 addition operator
- Basic automated tests

## Requirements

- C++17 compiler
- CMake 3.20+

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Run

```bash
./build/tiny2d_sandbox
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Roadmap

- Window and input system
- OpenGL rendering
- Scene management
- Collision detection
- Arena Survivor demo
