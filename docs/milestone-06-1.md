# M6.1 — Arena Survivor Gameplay Core

Baseline: main 9fc93ba3d18cafad82e3daeb17d1b3893a33691a (merged PR #5, including
M5.1 b7229d2). Branch: feat/gameplay-core. CMake version remains 0.4.0.
No engine core rewrite, tag, release, merge, or M6.2 work.

## Implementation

- CPU-only GameSession: read-only Scene/entities, playerId, health/alive, ticks/time,
  live enemy/projectile counts and actual spawned/fired/hit/kill counters.
- CombatConfig validates all simulation numbers; GameplayInput supports injected input.
- Complete ObjectId-keyed EntityState isolates gameplay data from GameObject.
- Normalized movement, player HP and integer-tick invulnerability, deterministic nearest
  targeting, automatic fire, projectile velocity/lifetime, enemy pursuit and ramped spawning.
- Ordered collision intent resolution, single-hit projectiles, one contact injury per tick,
  exact-once kill counting, deferred gameplay cleanup after Scene event delivery.
- SurvivorDemo is a separate Application adapter; original ArenaDemo remains untouched.
  Shared white texture, entity tint, dark grid, following camera and title statistics.

Sources: game/include/tiny2d_game/{GameSession,CombatConfig,GameplayInput}.h,
game/src/GameSession.cpp, sandbox/src/SurvivorDemo.{h,cpp}. See
[architecture](gameplay-architecture.md) and [combat rules](combat-rules.md).

## Build and run

For each variant below, configure, build, then run CTest:

```bash
cmake -S . -B build/m61-debug -DCMAKE_BUILD_TYPE=Debug -DTINY2D_OPENGL_TESTS=ON
cmake --build build/m61-debug --parallel
ctest --test-dir build/m61-debug --output-on-failure

cmake -S . -B build/m61-release -DCMAKE_BUILD_TYPE=Release -DTINY2D_OPENGL_TESTS=ON
cmake --build build/m61-release --parallel
ctest --test-dir build/m61-release --output-on-failure

cmake -S . -B build/m61-headless -DCMAKE_BUILD_TYPE=Release -DTINY2D_OPENGL_TESTS=OFF
cmake --build build/m61-headless --parallel
ctest --test-dir build/m61-headless --output-on-failure

./build/m61-release/tiny2d_sandbox --scene survivor-demo
./build/m61-release/tiny2d_sandbox --scene survivor-demo --frames 600
```

WASD moves; arrows offset the camera; Q/E zoom; Escape exits. No screen text HUD.
Finite mode ignores movement keys, advances exactly one tick/frame and prints real state.

## Verified 2026-11-02

Debug **21/21**, Release **21/21**, Headless **9/9**. Builds succeeded.
Debug/Release GPU tests used a real macOS OpenGL context. An initial sandboxed run was
unable to access macOS notification/context services and timed out; it was stopped and
the complete suites rerun successfully with access to the graphical session. Final logs
are in docs/data/m61; headless results are not substituted for GPU results.

All original 16 tests remain, including M41GPUTests, M5GPU, M5Arena, M51GPU,
RenderFailureTests, Scene, Collision and FixedTimestep. New tests:

- M61_combat: HP, Enter/Stay cooldown boundary, multi-enemy contact, death and halted firing,
  normalized movement, invalid config/input, nearest/tied aim, empty targeting, shot schedule,
  and equivalent 30/60/120 FPS partitions.
- M61_projectiles: single-hit and simultaneous multi-target resolution, multiple bullets killing
  once, expiry before collisions, ignored role pairs, cap recovery, overlap +X fallback,
  2400 projectile expiry/reuse cycles and 1200 two-projectile enemy kills.
- M61_spawning: safe initial positions/HP, chase velocity, spawn boundary/cap, two independent
  6000-tick sessions comparing creation identity, health, positions and all combat counters.
- M61GPU: actual player/enemy/projectile pixels, one shared-texture batch, enemy/projectile
  removal pixels, damage state/sprite synchronization, pending texture lifetime, context
  rejection, preflight recovery, cancel and GL error checks.
- M61Survivor: 600 real rendered frames with 600 fixed ticks.

The standalone gameplay CPU executable's dynamic dependencies are libc++ and libSystem;
there is no OpenGL/GLFW dependency. No real keyboard is needed for combat tests.

Release regression output:

```text
Triangle 30 frames: 1 draw
Immediate sprites 500 / 30 frames: 500 draws
Batch sprites 500 / 30 frames: 1 draw
Multitexture 5000 / 30 frames: 5 draws
Arena frames=120 ticks=120 draws=1 objects=55 contacts=7 events=170 created=2 deleted=2
Survivor frames=600 ticks=600 health=0 kills=6 enemies=8 projectiles=0 spawned=14 fired=13 hits=13 draws=1
```

The stationary survivor dies during the smoke run; death is persistent and simulation
continues safely. Automated fixture tests independently force immunity, expiry, kills
and reuse rather than relying on GUI input.

## Performance

Release benchmarks: 30 warmups + 300 measured samples per workload, fixed populations,
stationary separated circles and no spawn/fire opportunities during measurement. Each
population includes one player in addition to the enemies/projectiles. Pair counts are
N(N-1)/2 potential checks, not overlap or damage counts.

tiny2d_m61_benchmark measures complete fixedUpdate and, separately, a matching plain Scene
update with zero velocity. The latter includes integration, shape preparation, pair checks
and event bookkeeping; it is a collision-cost proxy, not an isolated narrow-phase timer.
Do not add it to fixedUpdate (which already includes Scene). tiny2d_m61_gpu_benchmark
measures SceneRenderer::render CPU submission with unchanged populations, without swap
or GPU completion synchronization. It does not measure GPU execution.

|Enemies / projectiles|Pairs|Fixed p50 / p95 (µs)|Scene p50 / p95 (µs)|Submission p50 / p95 (µs)|Draws|
|---|---:|---:|---:|---:|---:|
|8 / 8|136|2.625 / 2.709|2.292 / 2.375|13.125 / 20.292|1|
|24 / 32|1596|13.667 / 15.166|12.708 / 14.250|15.083 / 23.166|1|
|48 / 64|6328|36.375 / 41.166|34.916 / 39.458|18.334 / 23.084|1|

Raw samples: [CPU](data/m61/cpu-samples.csv), [submission](data/m61/render-samples.csv),
[environment](data/m61/environment.txt). Timing varies with scheduling and session load;
these are observations, not regression thresholds. The largest workload is 113 colliders
and 6328 pairs/tick. Current cost does not justify a broad-phase rewrite. Batch capacity
overflow is allowed to flush automatically; one draw is only observed for these populations.

## Limits and next milestone

Discrete collision can tunnel at high speeds or grazing trajectories. No collision layers,
avoidance, separation response, ECS, multithreading, world bounds or cross-platform bitwise
determinism guarantee. Grid decoration covers a finite area. Gameplay tests prove bounded
object counts and handle cleanup; GPU tests prove specific ownership/release paths, not
whole-process or driver leak freedom. No GPU memory profiler certification is claimed.

Keyboard/resize interaction is implemented but has not received a manual playtest in this
run. Current defaults allow the stationary player to die quickly; balancing remains open.

M6.2 should add the 90-second survival victory condition, Game Over, Restart, on-screen HUD,
richer feedback and balance adjustments. None of those systems are started here.
