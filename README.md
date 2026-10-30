# Tiny2D Engine

C++17 2D engine using macOS OpenGL Core and GLFW. M4 adds order-preserving indexed
batch rendering, up to eight texture slots, per-frame statistics and reproducible
CPU/GPU profiling. M0–M3 math, window/input loop, shader triangle, PNG textures,
immediate SpriteRenderer and Camera2D remain available.

## Build and run

Requires macOS, AppleClang, CMake 3.20+, GLFW 3.3+ and libpng (`brew install glfw
libpng`). Dependencies are local; CMake never downloads sources.

```sh
cmake -S . -B build/m4-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/m4-debug -j 8
ctest --test-dir build/m4-debug --output-on-failure
build/m4-debug/tiny2d_sandbox --renderer immediate --scene sprites --sprite-count 500 --frames 120
build/m4-debug/tiny2d_sandbox --renderer batch --scene multitexture --sprite-count 5000
build/m4-debug/tiny2d_sandbox --scene triangle --frames 4
cmake -S . -B build/m4-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/m4-release -j 8
build/m4-release/tiny2d_benchmark > trials.csv 2> environment.txt
```

If necessary add `-DCMAKE_PREFIX_PATH="$(brew --prefix)"`. Assets resolve relative
to the executable, independent of working directory. Default renderer is immediate;
Sprite count accepts 0–100000. Arrow keys pan, Q/E zoom, WASD change background,
Escape exits. Interactive VSync is preserved; only benchmark disables it.

Debug and Release pass 9/9 CTest tests including six real macOS OpenGL tests.
A graphical login is required. Explicit `-DTINY2D_OPENGL_TESTS=OFF` runs only
CPU/fake-driver tests and cannot establish rendering correctness.

500 identical-texture sprites are verified at 500 Immediate draws versus 1 Batch
draw. This is a draw-call result, not an FPS improvement claim. Benchmark uses
100/500/1000/5000 sprites, CPU submission and pixel-load workloads, warmup, five
repeats, CPU p50/p95 and asynchronous GPU queries when supported. It writes raw
CPU/GPU frame CSVs in the current directory; avoid overwriting earlier runs.

- [M4 implementation and reproduction](docs/milestone-04.md)
- [Batch lifecycle, state and blending contract](docs/batch-rendering-design.md)
- [Measured performance and raw data](docs/performance-m4.md)
- [Review fixes, coverage and limitations](docs/m4-review-notes.md)
- [M3 architecture and licensing](docs/milestone-03.md)
- [Historical M3 process timing](docs/performance-baseline.md)

Next: scene management, collision detection and an Arena Survivor demo; renderer
work follows profiling evidence and cross-driver regression coverage.
