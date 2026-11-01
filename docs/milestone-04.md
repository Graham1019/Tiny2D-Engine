# M4 — Batch rendering and profiling

Implemented on unmerged M3 eec95d3 (feat/texture-sprite-camera). No M0–M3 tests
removed. BatchRenderer and RenderStats are added alongside the immediate renderer.
See [design](batch-rendering-design.md), [performance](performance-m4.md), and
[review notes](m4-review-notes.md).

## Reproduce

```sh
cmake -S . -B build/m4-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/m4-debug -j 8
ctest --test-dir build/m4-debug --output-on-failure
cmake -S . -B build/m4-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/m4-release -j 8
ctest --test-dir build/m4-release --output-on-failure
build/m4-release/tiny2d_sandbox --renderer immediate --scene sprites --sprite-count 500 --frames 120
build/m4-release/tiny2d_sandbox --renderer batch --scene sprites --sprite-count 500 --frames 120
build/m4-release/tiny2d_sandbox --renderer batch --scene multitexture --sprite-count 5000 --frames 120
build/m4-release/tiny2d_benchmark > trials.csv 2> environment.txt
python3 scripts/summarize_m4.py trials.csv
```

Benchmark creates `m4-frames.csv` and `m4-gpu-frames.csv` in the current directory;
use a fresh output directory to preserve prior runs. Optional `--samples N` changes
measured frames per trial (default 120); 30 warmup frames and 5 repeats remain.

Both configurations build with AppleClang 21 and pass all 9 CTest tests in a real
macOS graphical session. Six tests require real OpenGL, three are CPU/fake-driver.
Headless configuration must explicitly set TINY2D_OPENGL_TESTS=OFF and does not
validate GPU behavior. Logs are committed under docs/data/m4.

M4GPUTests covers 0/1/500/1000/1001/5000 sprites, actual statistics, 3-texture
interleaving, 9-texture overflow, reuse, order in both directions, transforms,
camera translation/zoom, whole-frame RGBA comparison, analytical alpha, state
restoration, overflow rejection, invalid sequences, moves and GPU deletion.
RenderFailureTests now injects Batch shader/creation/upload/layout failures and
checks that all fake-driver resource owners are released. A failed frame upload
is tested for recovery. Existing M3 tests retain resize and input foundations.

Observed full-frame pixel maximum absolute channel difference: 0 in both orders;
allowed tolerance is 2/255 per channel, with no mismatched-pixel budget.
500 same-texture sprites: Immediate 500 draws; Batch 1 draw, 1 flush, 1 texture
bind, 72,000 uploaded vertex bytes. 5000-sprite multi-image smoke: 5 draws.

Three new original procedural PNG checker textures are dedicated under CC0-1.0.
No external image downloads or undocumented asset rights are involved.

Resume material: implemented order-preserving indexed batching with up to eight
textures on GLSL 330, reduced the verified 500-sprite draw count from 500 to 1,
and built real-driver pixel regression plus asynchronous GPU timer profiling.
Do not describe draw-call reduction as an FPS speedup.

## M4.1 targeted review follow-up

Built on 84523e226a318337cba25fe213bbd238099dee4b, same feat/batch-renderer
branch and PR #3. No M5 work or history rewriting.

- Explicit nonbinding Texture2D context validation before enqueue/automatic flush.
- Move assignment rejects any active operand; move construction still transfers.
- Sandbox creates only the selected renderer and removes redundant beginFrame.
- M41GPUTests adds dual unshared contexts, rejection atomicity, all move state
  combinations, and 9-texture overlapping RGBA regression with slot reuse.
- Debug and Release: 10/10 CTest passed, preserving all previous nine tests.
- Both transparent orders: 3 draws/flushes, 18 binds, max channel error 0/255.
- Requested 30-frame Sandbox runs: Immediate 500 draws, Batch 1, multitexture
  5000 sprites 5; triangle 4-frame run 1. Both zero-sprite modes report 0;
  triangle with --renderer batch reports 1 without creating a BatchRenderer.

Logs and new full benchmark data are in docs/data/m41; historical M4 data is
unchanged. [Performance comparison](performance-m41.md) documents hot-path costs.
Actual keyboard/Escape and physical minimization interactions were not performed;
input logic is unchanged and minimized displayed counts reset before size checks.
