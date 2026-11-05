# M6.3b — Combat feel, visual polish and profiling

Base main: 4be1df0ef4ed7ef46d407f1036895ecd519e4168 (PR #8 merge verified on
2026-11-03). Branch: feat/combat-polish-profiling. The user's M6.3a playtest confirmed
movement, difficulty and the game loop, so all five balance curves remain frozen.

## Implementation

- Fixed-capacity, sequence-addressed CPU combat event history, emitted during existing
  hit/damage/death resolution. No second collision pass or gameplay dependencies.
- Independent CPU CombatVFX: four-spark hits/deaths, two-square projectile trails,
  red damage edges and 3-pixel decaying camera feedback; no hit stop or gameplay edits.
- 128 effects, 561 preallocated world Sprite slots and four UI edge slots. Restart
  detects new Scene identity and clears old feedback. One world pass plus existing UI.
- Blue/player and red/enemy center marks, lower-contrast grid, green/red terminal accents.
- FrameRate uses a recent one-second render window, includes menu rendering, resets
  on long pauses/non-drawable/unfocused windows, and preserves stable completed values.

See [feedback](combat-feedback-design.md), [lifecycle](vfx-lifecycle.md), and
[performance](performance-m63b.md). BitmapFont, Renderer hot paths, object handles,
collision, 5400-tick survival/death precedence and restart contracts are preserved.

## Validation

Debug **38/38**, Release **38/38**, Headless **22/22**. All former 34/19 tests remain.
Logs in data/m63b. After strengthening GPU soak, the final GPU test was rerun in both
builds (gpu-soak-*.txt). No old assertions were removed or disabled.

New tests cover real hit/kill/damage emission, retained multi-tick history, overflow,
30,000 paired gameplay opportunities with/without VFX, 30,000 dense create/expire
updates, bounded slot counts, expiry, no colliders, 120 restarts, 60/120Hz FPS samples
and pause recovery. GPU pixels cover hit yellow, death orange over hit, fading/expiry,
trail residue after projectile deletion, damage edges, HUD independence, resize,
reset, wrong Context and preflight/cancel. A failing hit pixel exposed marker ordering;
markers now render before feedback. The corrected test passes.

GPU soak: 10,001 frames, recurring real kills while Playing, then terminal Victory
rendering with expired effects; retained white texture handle and no GL errors.
Existing M62 GPU tests also retain 150 restart/resource loops. Legacy commands confirm
1/500/1/5 draws and unchanged Arena 120 ticks, 55 objects, 7 contacts, 170 events,
2 created/2 deleted. See legacy.txt.

Build each configuration with:

```
cmake -S . -B build/m63b-debug -DCMAKE_BUILD_TYPE=Debug -DTINY2D_OPENGL_TESTS=ON
cmake --build build/m63b-debug --parallel
ctest --test-dir build/m63b-debug --output-on-failure
cmake -S . -B build/m63b-release -DCMAKE_BUILD_TYPE=Release -DTINY2D_OPENGL_TESTS=ON
cmake --build build/m63b-release --parallel
ctest --test-dir build/m63b-release --output-on-failure
cmake -S . -B build/m63b-headless -DCMAKE_BUILD_TYPE=Release -DTINY2D_OPENGL_TESTS=OFF
cmake --build build/m63b-headless --parallel
ctest --test-dir build/m63b-headless --output-on-failure
```

Run interactive Survivor without --frames; finite mode accepts --difficulty 1..5
and positive --frames exactly as M6.3a. No new CLI or gameplay mechanism is introduced.

## Performance and limits

Nine matched baseline/feature fixtures, 30 warmups and 100 samples each; 1800 raw rows
plus reproducible benchmark and summary scripts. Tables include every requested CPU
stage, Scene/collision proxy, render counters, populations, p50/p95/p99 and completion
wait. No pure GPU execution claim; no whole-process/driver leak certification.

Final CPU frame p95 for Lv.1/3/5: 70.583/65.500/67.167 us. Maximum-population fixture:
103.291 us (+36.6%); continuous burst: 130.542 us (+42.6%). The additional hundreds
of visual sprites increase world submission/upload cost, while world remains one
batch. This exceeds the warning threshold and is documented, not hidden by changing
fixture population. Menu p95 differences are noise/driver scheduling, not an optimization
claim: the menu renderer was unchanged. No broad-phase/backend rewrite was justified.

## Manual acceptance — pending user

- Smooth movement and unchanged Lv.1/3/5 difficulty feel.
- Clear hits/deaths without obscuring dense combat; readable damage and acceptable shake.
- HUD remains stationary; restart leaves no old effects; Victory/GameOver remain clear.
- Resize, minimize/restore, focus transitions and cross-display Retina behavior.
- FPS after spending time in menu and after long suspension.
- Perceived smoothness under simultaneous enemies, projectiles and VFX.

Automated CPU inputs and framebuffer pixels are not a human visual acceptance pass.
Discrete collision, finite decorative grid and no cross-platform bitwise guarantee remain.
VFX rings can drop old visual events under overload; gameplay is unaffected. No new
art assets, audio, buffs, maps, upgrade tree, general particle engine or release packaging.
Next suggestion: M6.3c Release & Portfolio Packaging after review and manual acceptance;
not started. Do not merge this PR automatically.

## M6.3b.1 review update

Death > Hit > Trail now protects active hit/death feedback from trail saturation.
CPU VFX/HUD preparation precedes both guarded World/UI passes. See
[review fixes](m63b1-review-fixes.md) for deterministic eviction/drop semantics,
exception/Context contracts, new tests and latest measurements. The M6.3b results
above remain historical; new data lives in data/m63b1 without overwriting them.
