# M6.2 — Arena Survivor playable loop

Baseline main: a90383fcfb44c37361c3d37dd4f43e0433767c35 (PR #6 merge).
Branch: feat/survivor-game-loop. Version remains 0.4.0. M6.3 is not implemented.

## Play

```sh
./build/m62-release/tiny2d_sandbox --scene survivor-demo
./build/m62-release/tiny2d_sandbox --scene survivor-demo --frames 600
```

WASD moves; Q/E zoom; arrows offset camera; Escape exits; R's press edge restarts
only after Victory/GameOver. Hold R never repeats. Five HP, automatic fire,
projectiles, deterministic enemy spawning and damage retain M6.1 rules.
Survive 5400 simulation ticks to win. Death has priority on the final tick.
Terminal world freezes and continues rendering under the result panel.

See [loop design](game-loop-design.md), [HUD rendering](hud-rendering.md),
[combat](combat-rules.md), [architecture](gameplay-architecture.md).

## Validation, 2026-11-02

Configure/build/CTest were run for Debug and Release with TINY2D_OPENGL_TESTS=ON,
and Release headless with TINY2D_OPENGL_TESTS=OFF. Commands:

```sh
cmake -S . -B build/m62-debug -DCMAKE_BUILD_TYPE=Debug -DTINY2D_OPENGL_TESTS=ON
cmake --build build/m62-debug --parallel
ctest --test-dir build/m62-debug --output-on-failure
cmake -S . -B build/m62-release -DCMAKE_BUILD_TYPE=Release -DTINY2D_OPENGL_TESTS=ON
cmake --build build/m62-release --parallel
ctest --test-dir build/m62-release --output-on-failure
cmake -S . -B build/m62-headless -DCMAKE_BUILD_TYPE=Release -DTINY2D_OPENGL_TESTS=OFF
cmake --build build/m62-headless --parallel
ctest --test-dir build/m62-headless --output-on-failure
```

Debug 25/25, Release 25/25, Headless 12/12. All original 21 registered tests remain.
The first restricted GPU attempt stalled during context setup; successful full GPU
runs used the logged-in macOS graphical session. Logs: [data/m62](data/m62/).

- Phase: actual 5400-tick CPU victory, early death, death at duration=1, invalid
  rules, complete terminal snapshots unchanged after 1000 additional calls, replay.
- Restart: 150 short complete loops, held/released key semantics, default eight
  enemies and first-shot cooldown, death/victory reset, new Scene identity, stale
  IDs, allocation failure preserves old session. CPU binaries create no GL context.
- HUD: actual HP 5/3/0, kills through 10/100, countdown boundaries, layouts at
  320x240/800x600, unchanged-state cache and terminal glyph generation.
- M62GPU: real pixels for full/empty HP bar, H bitmap, victory/game-over title,
  world camera movement/zoom independence, overlay disappears after restart,
  150 restarts with same registered texture, actual pass draw counts, logical resize,
  invalid UI asset preflight/cancel recovery and GL error check.
- Existing M51GPU/M61GPU retain context/texture keep-alive/cleanup and failure
  contracts. No SceneRenderer/BatchRenderer/Scene implementation changed.

The M61 projectile lifetime stress fixture removes its contact enemy after the
contact assertions, keeping subsequent expiry/reuse loops Playing. No assertions
were removed. All other original test sources remain unchanged. Historical M6.1
logs and milestone remain untouched; their post-death behavior is historical.

## Smoke and stability

Triangle 1 draw; Immediate 500 sprites 500; Batch 500 sprites 1; Multitexture 5000
sprites 5. Arena 120 frames/ticks, 1 draw, 55 objects, unchanged lifecycle metrics.
Stationary survivor ends at tick 492 with HP 0, 6 kills, 7 enemies, 0 projectiles,
13 spawned/fired/hits, 81.8 seconds remaining. World/UI each use 1 draw, total 2.
600 and 6000 frame runs retain the same terminal gameplay state; see sandbox.txt.
Terminal frames no longer imply simulation ticks. 6000-frame execution is a bounded
render soak, not an hours-long stability or GPU memory certification.

## Performance

[Raw 1500 measured samples](data/m62/performance.csv),
[summary and methodology](data/m62/performance.md). Five workloads: Playing,
48 enemies/64 projectiles, Victory, GameOver, and post-restart. 30 warmups plus
300 measured samples each. Largest world has 113 objects and 6328 potential pairs.
Fixed update p50 at that load: 22.292 us. Cached HUD p50: 0.250 us; observed changed
HUD maximum: 20.875 us. UI object counts: 225 Playing, 863 Victory, 885 GameOver.
World/UI remain one draw each; no capacity flush needed for these fixtures.
Overlay UI submission p50 is 74.812/66.688 us; full separate-pass submissions
80.104/71.791 us. CPU submit excludes GPU completion, clear, swap and simulation.
The benchmark world omits the demo's 82 grid sprites. Historical M6.1 measurements
are not a controlled same-session comparison; no performance gain is claimed.

Session and HUD replacements release their old CPU containers. The renderer's
registered texture handle is stable across 150 tested GPU restarts. No shader,
texture or buffer construction is on the restart path. No GPU memory analyzer was
used; no comprehensive driver/process leak certification is claimed.

## Remaining acceptance and M6.3

Real keyboard WASD/QE/arrows/Escape/R, prolonged R holding at the OS input layer,
interactive minimize/restore and cross-display Retina transitions have NOT been
manually verified. Automated input-edge and resize pixel tests do not substitute
for that playtest. Zero-framebuffer guard is code-reviewed, not forced by the GPU
fixture. Visual readability has CPU bounds and sampled pixel coverage, not a full
human visual acceptance pass. Default gameplay balance still needs human play.

M6.3 may improve visual effects and typography, profile HUD rebuild allocation and
collision under measured workloads, conduct longer resource profiling/playtests,
and prepare release packaging. Extreme custom numeric labels can outgrow the
compact HUD. No audio, particle engine, UI framework or new backend was introduced.
