# M1–M4.1 integration / v0.4.0 candidate

Validated 2026-11-01 (Asia/Shanghai). Awaiting user and ChatGPT Code Review; do not
merge, tag or publish automatically.

## Git baseline and topology

Initial main: `52587ef22ea2170d614f69cae8520893aa65d95a`.
Final cumulative implementation baseline: `cee42f3e34693d2777d5f4545c697af62029b67d`
(`origin/feat/texture-sprite-camera`). `integration/m1-m4` starts at that commit.
The release-preparation commit adds documentation/evidence and changes only the
CMake project version from 0.1.0 to 0.4.0; engine, Sandbox, Benchmark and tests are
byte-for-byte unchanged from the baseline. The final integration commit SHA is
available in the PR head and `git rev-parse HEAD`; the baseline SHA here identifies
exactly the implementation measured, without a self-referential commit hash.

GitHub and fetch confirmed:

| PR | Actual merge | Merge commit |
|---|---|---|
| [#1](https://github.com/Graham1019/Tiny2D-Engine/pull/1) | M2 → feat/window-input-loop | edf114a641e731b74630162fb18401e0b03a8c7f |
| [#2](https://github.com/Graham1019/Tiny2D-Engine/pull/2) | M3 → feat/shader-triangle | 15de44606caeb68824cb93bb8b54699981c859b2 |
| [#3](https://github.com/Graham1019/Tiny2D-Engine/pull/3) | M4.1 → feat/texture-sprite-camera | cee42f3e34693d2777d5f4545c697af62029b67d |

The implementation chain is main → `d1be1c3` (M1) → `97dbf79` (M2) → `eec95d3`
(M3) → `84523e2` (M4) → `5c6a1af` (M4.1), joined by PR #3 at `cee42f3`.
`git merge-base --is-ancestor` returns 0 for main, all original M1/M2/M3 commits
and `5c6a1af6a7144e838d31f3b7b2ae41be7a53e8e8` against the cumulative baseline.
PR #1/#2 merge commits are NOT ancestors (exit 1): they landed on lower stacked
branches after higher branches were created. Their trees match `97dbf79` and
`eec95d3` respectively (empty diffs), so no implementation is missing. No cherry-pick,
rebase, history rewrite or redundant merge is needed or performed.

## Module coverage

| Milestone | Preserved implementation / checks |
|---|---|
| M1 | GLFW Window/Input/Application, deltaTime, framebuffer resize, logical/Retina sizes, RAII |
| M2 | ShaderProgram, VBO/VAO, context validation, compile/link errors, external RGB triangle shaders, failed allocation cleanup |
| M3 | libpng ImageLoader, Texture2D, SpriteRenderer, Mat4, Camera2D, orthographic projection, alpha, camera input |
| M4 | BatchRenderer/RenderStats, stream VBO/fixed EBO, slots/flush, GLSL 330 constant sampler indices, ordered transparency, Immediate comparison, CPU/GPU Benchmark |
| M4.1 | Texture context validation before queue mutation, unshared contexts, cross-slot full-frame pixels, selected renderer initialization, safe moves |

## Build and CTest

Environment: Apple M5 Pro, arm64, macOS 26.5.1 (25F80), AppleClang
21.0.0.21000101, CMake 4.4.3, GLFW 3.5.1, libpng 1.6.59, system OpenGL.
Both clean isolated Debug and Release configurations used `TINY2D_OPENGL_TESTS=ON`.
Both built the static library, Sandbox, Benchmark and all test executables without
compiler warnings. Dependencies resolved locally, with no installation needed.
CMake paths use source/target variables; detected SDK/Homebrew paths in generated
logs are not hard-coded runtime configuration. Assets copied correctly; root/build
working-directory smoke tests passed. Build products remain ignored.

| Test | Debug | Release |
|---|---|---|
| M3CPUTests | PASS | PASS |
| Vector2Tests | PASS | PASS |
| RenderFailureTests | PASS | PASS |
| M3GPUTests | PASS | PASS |
| TriangleRegression | PASS | PASS |
| OpenGLResourceTests | PASS | PASS |
| SandboxSmokeRoot | PASS | PASS |
| SandboxSmokeBuild | PASS | PASS |
| M4GPUTests | PASS | PASS |
| M41GPUTests | PASS | PASS |

`ctest -N` listed all 10 in both builds. Seven tests per build use real macOS
OpenGL. The first restricted run stalled in graphical tests and was stopped;
its partial log remains locally in `build/integration-evidence/debug-ctest.txt`.
The results above are the completed desktop-access reruns, not headless substitutes.
[Debug log](data/integration-m1-m4/debug-ctest-gui.txt),
[Release log](data/integration-m1-m4/release-ctest-gui.txt),
[pixel log](data/integration-m1-m4/pixel-release.txt).
Logs replace the checkout's personal path with `<repo>`.

Real-driver and injected tests cover allocation/deletion, shader errors, texture
ownership and pixel-store restoration, wrong context/thread rejection, moves,
1000/1001 capacity boundaries, slot overflow, transparent order and failed-frame
recovery. M4.1 full-frame comparison: both orders maximum RGBA error 0 (limit 2/255),
3 draws/flushes and 18 binds; dual unshared contexts and move state table passed.

## Sandbox acceptance

Release, 30 frames each, exit 0; logs are in `data/integration-m1-m4/sandbox-*.txt`.

| Scene | Renderer / sprites | Final-frame draws |
|---|---|---:|
| Triangle | triangle | 1 |
| Sprites | Immediate / 500 | 500 |
| Sprites | Batch / 500 | 1 |
| Multitexture | Batch / 5000 | 5 |
| Sprites | Immediate / 0 | 0 |
| Sprites | Batch / 0 | 0 |

Manual WASD, arrows, Q/E, Escape, native close, interactive resize,
minimize/restore and cross-display Retina switching were NOT verified in this
integration session. Finite-frame exit is not Escape acceptance. Automated resize
and pixel checks do not replace these manual checks.

## GPU lifetime and remaining debt

GLObject captures the creation context, owns the handle immediately and cleans up
construction failures. Shader temporary objects are deleted after linking. Sandbox
derived GPU members die before Application's Window. GPU destruction requires the
live owning context current on the main thread and terminates on contract violation.
Batch move assignment rejects either active owner before mutation, validates inactive
contexts and transfers ownership. Active move construction transfers the pass;
active destruction cancels queued work and restores state.

Textures are borrowed through queued GL handles and must outlive flush; context
validation does not extend their lifetime. Exclusive begin/end passes prohibit
incompatible external GL changes. See [current contract](batch-rendering-design.md).
Remaining debt: texture lifetime management, borrowed VBO lifetime, manual GUI
acceptance, other drivers/platforms, GPU memory-profiler soak, repeated-session
tail-latency analysis, repository-wide license choice and macOS GPU CI infrastructure.
No new rendering risk was introduced by integration; these existing constraints remain.

## Performance and reproduction

Historical [M4](performance-m4.md) / [M4.1](performance-m41.md) reports and their
raw data remain unchanged. Both summary scripts ran successfully. Historical
500-sprite CPU p50: Immediate 489.92 µs, Batch 90.67 µs (~5.40× CPU submission ratio).
500 → 1 draws means 99.8% fewer calls, not 99.8% faster frames. GPU timing does not
establish whole-frame FPS improvement.

This integration reran the unchanged default Benchmark in the new
`docs/data/integration-m1-m4` directory, separately from tests and Sandbox.
See its environment, raw frame CSVs and trials for the current measurements.
Reproduction commands are in README; use a fresh output directory. Implementation
is bound to `cee42f3e34693d2777d5f4545c697af62029b67d`, with CMake version 0.4.0,
Release and the toolchain above. No renderer or benchmark changes were required.

## Release and M5 handoff

`v0.4.0` is a candidate only. After this PR is reviewed, explicitly approved and
merged, validate updated main before asking the user to authorize its Tag/Release.
See [candidate release notes](release-notes-v0.4.0.md).

Only after integration reaches main, fetch with temporary HTTP/1.1 settings and
create `feat/scene-collision` from updated `origin/main`. M5 scope: Scene Management,
GameObject/Transform2D, Collision2D, Fixed Timestep, Texture Lifetime and Interactive
Arena Prototype. Start by defining ownership and fixed-update contracts with tests;
do not carry new M5 work on this integration branch. No M5 code is implemented here.

### Integration measurement results

The default run produced all 9600 CPU and 9600 GPU samples (80 trials); timer
queries were available, framebuffer 1600×1200, logical size 800×600, capacity 1000,
slots 8, VSync off. [Summary](data/integration-m1-m4/summary.csv) uses the same
median-of-five trial percentiles as historical reports.

500-sprite CPU workload: Immediate p50/p95 495.75/525.12 µs; Batch 87.83/104.29 µs,
with 500/1 draws respectively (~5.64× CPU submission p50 ratio). GPU p50 is
232.62/163.54 µs. This reproduces the core batching benefit, not exact historical
latencies. Pixel workload remains noisy: 5000 Batch CPU p50 is 417.42 µs versus
historical M4.1 279.42 µs; Immediate is 5403.38 versus 4518.58 µs. These unchanged-path
session differences are retained in raw data and cannot establish a code-caused
regression or a guaranteed FPS gain. Repeated sessions/profiling remain necessary.
