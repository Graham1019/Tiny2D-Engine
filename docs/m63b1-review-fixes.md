# M6.3b.1 review fixes

PR #9, branch feat/combat-polish-profiling; before SHA
54782992f01226b5ee619b1531d6825424546bb8. No main changes, history rewrite,
new PR, merge, gameplay tuning or M6.3c work.

## Priority protection

The former circular writer allowed 64 projectile trails per tick to replace a 0.30s
death burst after only a few ticks. EffectKind now orders Death > Hit > Trail. Each
accepted insertion receives a monotonic creation sequence. A fixed-array scan chooses:

1. First inactive slot by ascending index.
2. Lowest-priority eligible active type, then oldest creation sequence.
3. If no lower type exists, oldest equal-priority instance.
4. If all slots have higher priority, reject the incoming instance.

Equal sequences retain the first index encountered. No dynamic allocation is added;
128 effects, 561 world Sprite slots and four UI edges remain unchanged. Player damage
keeps its independent timer. Trail/Hit/Death render orders are 2/3/4, so a retained
important burst is also drawn above trails. Same-type ordering is stable slot order.

Drop semantics: one per rejected incoming effect OR evicted active effect, never both;
free/expired reuse adds zero. Lost unconsumed entries in the 512-event combat ring add
the exact lost count. Re-consuming a tick adds nothing. Reset/new Scene token clears
all effects, creation sequence, drop count and event cursor. Death overflow can still
replace older deaths; lower-priority trails cannot do so.

## Render exception safety

SurvivorPresentation prepares VFX and HUD CPU Scenes, then camera viewports, before
starting either pass. Invalid VFX viewport rejects before mutation. HUD allocation
failure retains the old cached Scene; rerunning preparation with valid inputs succeeds.
The application still skips minimized/zero framebuffer and handles menu separately.

RenderPass calls begin in its constructor. A failed begin constructs no guard and
cannot cancel someone else's existing pass. end marks completion only after success.
On unwinding, the noexcept destructor checks SceneRenderer::passActive(): preflight
failures leave active work to cancel; runtime failures that already closed the pass
are not cancelled twice. The read-only accessor introduces no renderer hot-path change.
Original exceptions propagate. Both World and UI use the same guard. World failure
prevents UI from starting; UI failure preserves completed world work. Whole-frame
statistics are returned/published only after both passes finish.

Cancel requires the owning GL context. A wrong-context end/cancel is still rejected;
the destructor suppresses the cleanup exception to preserve the original error, and
DOES NOT claim recovery. The owner must restore its context and explicitly cancel
before reuse/destruction. Application rendering does not switch contexts inside passes.
Cancel drops only pending commands: already flushed draws cannot be rolled back.

The title again shows HP, kills, active VFX and actual total draws alongside the
existing recent-window FPS. No FPS timing semantics changed.

## Tests

M63b_priority (CPU/headless) tests all nine incoming/existing priority combinations,
lowest-type-before-age selection, deterministic oldest eviction, free-slot reuse,
exact drop accounting including lost event history, real hits/deaths under continuing
64-projectile trails, natural expiry, unchanged gameplay counters, two independent
VFX instances' slots/Sprite output and restart sequence reset.

M63bGPU adds real framebuffer pixels before and after six ticks of saturated trails,
then expiry pixels, for both hit and death. Existing alpha, resource, resize and
10,001-frame soak assertions remain.

M63b_render_recovery tests World/UI invalid submit with original diagnostic preserved,
GL bindings/blend restoration, clean next-frame pixels, completed-world preservation,
capacity-flushed work remaining visible, nested-begin failure, real GL error during end,
wrong context with explicit later cleanup, and pending texture owners released.
The exact production preparation helper is tested with invalid VFX dimensions and
injected std::bad_alloc during HUD preparation; neither starts a GPU pass and both retry.
RenderFailureTests additionally injects upload failure inside end with the actual guard.
Debug consumes a deliberately pending GL error at texture validation; Release consumes
it at batch upload. Assertions distinguish the real diagnostics instead of assuming
identical validation points.

Final results: Debug **40/40**, Release **40/40**, Headless **23/23**.
The GPU suite includes the retained 10,001-frame soak. Legacy draws remain
1/500/1/5; Arena remains 120 ticks, 55 objects and 170 events.
Latest results and raw logs are in data/m63b1. All former 38 graphical / 22 headless
tests are retained. The existing 120-restart and long CPU/VFX stresses still run.

## Performance

See [latest distributions](data/m63b1/summary.md). Before and after run the same
benchmark against exported 5478299 and the fixed tree. Eleven workloads, 30 warmups
plus 100 samples: original nine, isolated saturated 64-projectile trails, and first
frame after each restart. Restart setup (advance old quiet session to terminal,
recreate) is outside timing; VFX reset/consume, HUD rebuild and first-frame rendering
are inside. Both versions use identical gameplay fixtures and input.

vfx_consume_us isolates event consumption/trail insertion from vfx_update_us aging;
it is an insertion-path measurement, not a per-single-effect microbenchmark. Full
frame, fixed update, presentation, world/UI submission, p50/p95/p99, active effects,
cumulative drops, actual Sprite/draw/upload counters are recorded. The simple O(128)
scan increases saturated insertion cost; it allocates no per-effect storage. More
high-priority effects may survive, intentionally changing the visual workload. No
renderer speedup is claimed. GL finish waits remain CPU waits, not pure GPU timings.

Measured CPU frame times (microseconds, p50 / p95 / p99):

|Workload|Before|After|
|---|---|---|
|Lv.3|58.459 / 64.083 / 89.792|57.542 / 69.166 / 87.416|
|48 enemies + 64 projectiles|97.625 / 100.708 / 123.041|102.209 / 112.042 / 126.500|
|8 kills/tick|125.042 / 129.709 / 132.792|125.375 / 128.708 / 144.417|
|Saturated trails|81.208 / 83.292 / 107.083|85.625 / 91.750 / 110.125|
|Victory|101.125 / 103.459 / 110.125|98.875 / 101.208 / 109.500|
|GameOver|104.208 / 108.416 / 111.833|102.500 / 110.042 / 112.166|
|Restart first frame|45.250 / 46.334 / 57.583|44.833 / 51.042 / 55.041|

At saturation, insertion-path p95 rises from 0.292 to 7.250 us; aging p95 is
0.167 us and presentation p95 is 2.250 us after the fix. Full frame p95 rises
10.2%; the 48/64 fixture rises 11.3%. This is the expected cost of scanning at
most 128 slots for each insertion. No per-effect allocation or new GPU resource
is involved. Both fixtures retain one world and one UI draw. Saturated trails
submit 674 total sprites / 97,056 vertex bytes; the 48/64 fixture submits 770 /
110,880. Small reductions elsewhere are measurement variability, not claimed gains.

Reproduce via `sh tests/profiling/reproduce_m63b1.sh`; summarize with
`python3 tests/profiling/summarize_m63b1.py`. Historical data/m63b files are untouched.

## Remaining acceptance

Human high-density visual acceptance, real minimize/restore/Retina behavior and camera
comfort remain user checks. Context-loss recovery is not automatic. Same-priority
overload still drops older instances. No full driver/GPU memory-leak certification.
Review fixes and manual acceptance precede any M6.3c work.
