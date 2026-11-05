# M6.3b performance methodology

Baseline is merged M6.3a 4be1df0ef4ed7ef46d407f1036895ecd519e4168.
The baseline was exported using git archive to a separate temporary directory, built
Release, and linked against the same benchmark source without M63B. The feature build
uses M63B to enable presentation. Nine fixtures use identical setup, inputs and 30
warmups plus 100 measured samples. macOS 26.5.1, arm64, real system OpenGL. Builds and
tests precede final sequential benchmark runs. These are local observations, not
statistically controlled claims of GPU/driver speed improvements.

Fixtures: Lv.1/Lv.3/Lv.5 Playing with movement (1,0); quiet 48 enemies + 64 long-lived
projectiles; eight real kills per tick; quiet Victory, GameOver, restarted quiet session;
and menu. Burst fixture creates enemies/projectiles outside timed frame work in BOTH
versions. This isolates their actual combat resolution and presentation. No baseline
is compared to a different gameplay population. Production difficulty is not changed.

Raw records: data/m63b/baseline.csv and polish.csv (900 rows each). Summary:
[data/m63b/summary.md](data/m63b/summary.md). Recompute with
`python3 tests/profiling/summarize_m63b.py`.

Timing boundaries:

- fixed_us: actual fixedUpdate, excludes clear and fixture construction.
- scene_proxy_us: separate frozen-layout Scene update with matching initial colliders;
  includes shape preparation/pair checks, not an isolated collision timer or a subspan
  of the actual moving GameSession. Do not add it to fixed_us or frame_cpu_us.
- vfx_update_us: aging and event/trail consumption; vfx_present_us: existing Scene slot
  updates. No VFX work executes in the menu fixture.
- hud_us: HUD or menu update, including real cache invalidation.
- world_us/ui_us: CPU submission, excluding explicit completion synchronization.
- frame_cpu_us: clear + simulation + presentation + HUD + two-pass submission, excludes
  proxy, fixture setup, sample serialization, swap, event polling and glFinish.
- finish_wait_us: CPU time blocked in glFinish. NOT pure GPU execution time.
- frame_completed_us: timed frame plus explicit completion wait. Reported reciprocal
  throughput excludes display/vsync and is NOT the title's interactive FPS.

RenderStats are copied per pass: real draws, submitted sprites, batch flushes, texture
binds and uploaded vertex bytes. Summary records p50/p95/p99 and runtime population
peaks. World/UI totals must be summed; menu has no world pass. Stats count renderer
texture binds, not every GL state restoration bind. No timer-query instrumentation or
new GPU backend was added.

The maximum-population and continuous-burst fixtures exceed the 20% p95 warning
threshold: they add hundreds of visible sprites and upload bytes while retaining one
world batch. World submission, not simulation or VFX aging, is the dominant added
cost. The final Lv.1/3/5 p95 changes are +10.4%/-4.2%/+10.9%; small negative changes
are measurement variability, not a claimed optimization. The detailed table records absolute costs, so a percentage
increase from a tiny baseline is not presented as frame-budget failure. No unmeasured
renderer redesign was introduced. Preallocated VFX slots prevent per-hit object churn;
existing SceneRenderer's command preparation remains a potential future profiling area.

FPS fix: FrameRate counts render intervals in an independent one-second window for
menu, playing and terminal frames. First sample displays zero until a full window;
non-drawable/unfocused samples, clock regression, and gaps over one second reset it.
Restart does not include menu uptime in a lifetime denominator. Short windows preserve
the last completed value. CPU tests inject exact times, pauses and 60/120 Hz streams.

## M6.3b.1 review update

Death > Hit > Trail now protects active hit/death feedback from trail saturation.
CPU VFX/HUD preparation precedes both guarded World/UI passes. See
[review fixes](m63b1-review-fixes.md) for deterministic eviction/drop semantics,
exception/Context contracts, new tests and latest measurements. The M6.3b results
above remain historical; new data lives in data/m63b1 without overwriting them.
