# M5.1 review fixes

Baseline: M5 `9bbea068e43fbabd279484a97ba0b518a015e922`, same `feat/scene-collision` branch and PR #5. No M6 feature work. Original M5 results remain under `docs/data/m5`; new evidence is under `docs/data/m51`.

## Exception safety

Previously `render` caught `submit` failure and called `end`, which flushed earlier valid commands even when a later sprite was invalid. Retrying a transparent scene could therefore blend twice.

`prepare` now snapshots stable render order, checks each live handle, Transform, Sprite, asset presence/non-nullness, texture context, effective size, finite tint and all four world-space vertices. The internal CPU helper `detail::spriteCorners` is shared with BatchRenderer::draw, preserving the exact matrix arithmetic and rejection criteria. Multiplication overflow and finite inputs whose computed vertices overflow are rejected. No GL bindings or draws change during successful-context preflight. Resource lookups are explicitly diagnosed as `missing scene texture asset`.

Prepared commands borrow texture pointers from an owned, deduplicated resource set. Preparation failure destroys this local set. `render` prepares before `begin`, so predictable Scene input failures do not create an active pass or reset renderer statistics. A valid scene then begins, submits and ends normally.

Explicit `begin/submit/end`: each submit preflights its entire Scene first. Validation or owner-set allocation failure preserves all previous queued commands, statistics and frame owners; callers may fix and retry, end the previous valid commands, or cancel. Once drawing begins, a runtime draw/upload failure attempts cancel of the entire pending pass, preserving the original exception. The convenient render wrapper similarly cancels on failure; it never calls end merely to recover from submit failure.

`BatchRenderer::cancelFrame` requires an active pass and its owning current context, clears pending CPU vertices/texture handles, restores saved GL state, and ends the pass without issuing a draw. `SceneRenderer::cancel` then releases frame owners. Wrong-context cancel/end rejects before mutation, retaining owners and the pass until the caller restores the context. Cleanup failures do not replace the original rendering exception. Destruction still requires the live owning context.

**Cancel cannot undo completed automatic Flushes.** Capacity/texture-slot overflow may already have drawn. Driver runtime errors, including errors reported after glDrawElements, do not offer framebuffer rollback. Stats retain submissions and completed draw accounting after cancellation; canceled submittedSprites are not presented as rendered sprites. Preflight only provides the no-draw guarantee for predictable Scene input failures. It is not a GPU transaction. External mutation of GL state or moving a registered Texture2D's resource during a pass remains outside the API contract.

## Scene identity

ObjectId now contains `{uint64_t sceneId, uint32_t index, uint32_t generation}`. A process-local atomic counter allocates Scene identities with relaxed compare/exchange (uniqueness needs no cross-object memory ordering). Zero is invalid. UINT64_MAX is a permanently exhausted sentinel: allocation throws rather than wrapping. Scene identity is not an address, never changes on Clear and is not reused after destruction. Scene remains noncopyable/nonmovable. Tokens are runtime handles, not persistent serialization IDs.

find validates sceneId before indexing; destroy delegates to find. Equality and lexicographic ordering include the token, maintaining strict ordering and complete collision pair identity. Generation independently protects reuse within one Scene, including the existing retirement policy. Existing short-lived borrowed-pointer rules remain. Scene creation allocation failures publish no object; an unused expanded slot may remain, which is harmless. Update/event callback failures still provide cleanup and usability, not whole-step rollback.

## Texture lifetime

Registry owns shared const Texture2D. Each prepared set and active-pass set is keyed by `const Texture2D*` for identity comparison only; every entry owns a shared_ptr. Aliased asset keys deduplicate to the same frame owner. Old and replacement textures under one key have different identities and both survive until end/cancel. Command pointers are valid only while these owner sets live. No ownership cycle is introduced.

Owner-set allocations occur before batch mutation. Persistent frame owners scale with distinct textures rather than sprites (100 shared sprites / two aliases retain one frame owner). Temporary preparation/merge copies scale with unique resources, not individual sprites. Per-sprite command storage is one reserved contiguous vector; ordered owner nodes allocate per unique texture. The internal friend test probe observes owner count/preflight timing without expanding the runtime public API.

## Tests and measurements

The existing 15 tests remain registered. M51GPU adds real-context whole-frame RGBA comparisons, GL bindings/blend-state checks, trailing invalid assets, zero/overflow/NaN geometry and tint, alpha retry, explicit submit recovery, capacity + slot overflow preflight, deduplicated owners, asset replacement/removal, wrong-context cancel/end, actual texture deletion and already-flushed cancellation semantics.

Scene CPU tests retain 20 × 1000 lifecycle stress and add cross-Scene rejection, Clear identity, placement-new address reuse, collision identity, callback exception cleanup, Clear during update, and next-step participation. Allocation failure injection checks the three first-create allocation stages. RenderFailureTests additionally injects GL upload failures through SceneRenderer, checks original error propagation, no recovery flush, usability and weak_ptr release.

Build/test results and timings are appended after validation. The M4 geometry helper extraction touches draw setup, so the full 9600 measured-sample renderer benchmark is rerun rather than assuming performance neutrality. Scene benchmark reports preparation separately from complete render submission, 30 warmups + 200 measured frames, repeated twice. Total render already includes its own preparation; it must not be added to the standalone preflight figure. These are CPU submission costs, not GPU completion times.

Manual keyboard, resize/minimize acceptance remains unchanged from M5. Token exhaustion and 2^32 generation retirement are reviewed guards, not exhaustively iterated in tests. No claim of full transactional recovery, leak-profiler certification or cross-platform bitwise determinism.

## Verified results (2026-11-02)

Debug 16/16, Release 16/16 (real macOS OpenGL); Headless 6/6. All configure/build steps succeeded. M3/M4/M41 pixel/context regressions remain passing. Scene allocation-failure and GL runtime-failure injection pass. Sandbox draw counts: Triangle 1, Immediate 500 = 500, Batch 500 = 1, Multitexture 5000 = 5. Arena unchanged: 120 ticks, 55 objects, 7 contacts, 170 events, 2 created / 2 deleted, 1 draw.

Scene measurements (two runs, median µs; 200 measured frames per workload per run):

|Sprites|Textures/keys|Preflight|Total render|Draws|Frame owners|
|---:|---:|---:|---:|---:|---:|
|100|1/1|4.125 / 4.125|25.959 / 26.458|1|1|
|500|1/1|18.916 / 19.083|58.042 / 59.625|1|1|
|1000|4/4|38.25 / 38.709|94.833 / 95.542|1|4|
|1000|4/32|40.083 / 40.25|95.75 / 95.459|1|4|

M4 path full benchmark: 120 samples × 5 repeats × 16 workloads = 9600 samples. Current-session retained integration-release binary provides a control; historical M4.1/M5 CSVs are untouched. Median of trial CPU p50s (µs):

|Workload|Sprites|Renderer|Control|M5.1|Change|
|---|---:|---|---:|---:|---:|
|cpu|500|immediate|485.8|489.6|+0.8%|
|cpu|500|batch|96.0|86.0|-10.3%|
|cpu|5000|immediate|4605.7|4539.3|-1.4%|
|cpu|5000|batch|280.5|288.5|+2.9%|
|pixel|500|immediate|494.6|490.0|-0.9%|
|pixel|500|batch|97.6|88.7|-9.1%|
|pixel|5000|immediate|4599.1|4591.2|-0.2%|
|pixel|5000|batch|331.4|360.8|+8.9%|

Historical M4.1 pixel/5000/batch CPU p50 median was 279.4 µs; M5.1 is 360.8 µs. Compare the current-session control above before attributing that historical difference to code. Driver scheduling, GPU backpressure and session workload remain confounders; no formal performance-improvement claim. Preflight adds a measured second CPU geometry pass intentionally; validation is not removed to save time. New raw measured samples are retained. The baseline executable is the retained integration artifact, not a newly reconstructed checkout.
