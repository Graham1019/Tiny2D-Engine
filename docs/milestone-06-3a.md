# M6.3a — Difficulty selection, pacing and menu

Baseline: 8d870232805f876303a59c3947cc90c3a40b1932, merged PR #7.
Branch: feat/difficulty-system. No tag, release, merge or M6.3b work.

See [difficulty formulas/table](difficulty-design.md), [menu flow](menu-flow-design.md),
[performance methodology and results](data/m63a/performance.md).

## Build / run

Configure with cmake -S . -B build/m63a-debug -DCMAKE_BUILD_TYPE=Debug
-DTINY2D_OPENGL_TESTS=ON, then cmake --build build/m63a-debug --parallel and
ctest --test-dir build/m63a-debug --output-on-failure. Repeat with m63a-release /
Release / ON and m63a-headless / Release / OFF. Headless excludes graphical tests;
the gameplay library itself has no GLFW or OpenGL dependency.

```
./build/m63a-release/tiny2d_sandbox --scene survivor-demo
./build/m63a-release/tiny2d_sandbox --scene survivor-demo --difficulty 3 --frames 600
./build/m63a-release/tiny2d_m63a_benchmark
./build/m63a-release/tiny2d_m63a_benchmark --stress
```

## Automated validation (2026-11-02)

Debug 34/34, Release 34/34, Headless 19/19. All 25 former graphical-suite tests
and all 12 former headless tests remain registered. Final logs live in data/m63a;
additional GPU assertions were rerun in both configurations in gpu-final-*.txt.

- Difficulty: full default identity, preserved combat invariants, strict curves,
  concrete quantization table, invalid enum and raw input rejection.
- Flow: no session/ticks in menu, default/selection/held edges, applied level lock,
  focus loss rearming, allocation failure on start and restart, 25 actual terminal
  restart loops, fresh IDs/counters and level-specific first-shot boundary.
- Pacing: five real session pairs, 6400 update opportunities, all counters and logical
  positions compared without Scene tokens; 30/60/120 FPS partitions; stationary
  death; controlled actual spawn opportunities over 5400 ticks; terminal freeze;
  real projectile/enemy velocities, fire count per deadline, bounded populations,
  single impact/score, 36-tick immunity and death priority on tick 5400.
- GPU: actual OpenGL pixels for default and changed menu highlighting, bitmap font,
  Enter transition, player world, difficulty label pixels, level cache changes,
  camera pan/zoom independence, GameOver and Victory restart, retained texture,
  resize, wrong-context rejection, per-pass draw accounting and GL errors.
- CLI: four rejected invalid levels and explicit Lv.5 finite run. Existing Survivor
  smoke retained, now implicitly starts Lv.3. All five levels also ran 600 frames.
- Legacy: triangle/immediate/batch/multitexture draw counts 1/500/1/5; Arena remains
  120 ticks, 55 objects, 7 contacts, 170 events, created/deleted 2/2, one draw.
  See sandbox.txt. M41/M51 and M61/M62 resource and gameplay regressions pass.

## Performance / limits

Raw normal and HP-only stress CSVs each contain five 5400-opportunity runs. Normal
runs freeze on death; stress runs reach 5400 actual ticks. Spawn/kill checkpoints,
active peaks, fixed p50/p95, HUD/menu cost, separate CPU submissions and completion
waits are documented. Existing M62 maximum-population benchmark was also rerun.
Object caps remain 48/64; Renderer/Scene/Collision hot paths are unchanged.
Text may exceed one batch in terminal layouts; actual draw totals sum both passes.
No GPU timer query or process-wide GPU leak profiler was used.

## Manual playtest checklist — not performed

Automated injected input and OpenGL pixels do not certify real OS keyboard behavior
or human readability. User acceptance remains needed for:

- Launch/default Lv.3; 1–5 highlighting; held Enter starts once; Escape exits.
- Applied HUD level and Lv.1 vs Lv.5 enemy speed/spawn density/fire feel.
- WASD, Q/E and arrows during play; HP zero and 90-second victory in human play.
- R preserves level; held R; focus loss/recovery with keys held.
- Resize at 320x240/800x600, minimize/restore, Retina transitions and no catch-up.
- Subjective balance, typography and visual polish.

No optional M return-to-menu shortcut. Discrete collision and finite decorative grid
retain existing limits; no cross-platform bitwise determinism guarantee. Follow-up
review and playtesting should precede any balance adjustment or M6.3b. Random drops,
random maps, upgrade trees, audio and release packaging remain out of scope.
