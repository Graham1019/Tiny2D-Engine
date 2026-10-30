# M4 review notes

- Fixed M3 alpha accumulation with separate RGB/alpha blend factors in both paths.
- Cached uniform locations without suppressing public missing-uniform diagnostics.
  Immediate private hot path avoids repeated uniform-location and current-program
  queries; remaining public VertexArray checks are intentionally retained.
- Debug-only GL prechecks coexist with always-on operation-result checks. Compile,
  link, resource allocation, upload and draw diagnostics were not disabled.
- Added real-driver upload tests on texture unit 3 with nondefault unpack
  alignment, row length, skips and PBO binding; all are restored.
- Sandbox frame draw count resets before framebuffer-size checks, preventing
  stale statistics for minimized/zero-render frames and triangle mode.
- All eight earlier CTest entries remain; M4GPUTests is the ninth. Failure tests
  now additionally exercise Batch constructor cleanup and upload recovery.
- Apple driver initially warned about unused sampler slots with incomplete
  textures. All unused sampler uniforms now alias complete unit zero. Final
  benchmark stderr has no such warning; first exploratory data was replaced.
- Benchmark is distinct from M3's process-wall-clock smoke timing. Final sampling
  runs separately from GPU regression tests; no specific FPS gain is promised.

## Limits / M5

Only Apple M5 Pro / macOS 26.5.1 was measured. Other GLSL 330 drivers need CI pixel
runs. No external GPU profiler or long-duration memory profiler was run: resource
checks use glIs* after deletion plus deterministic ownership failure injection.
Manual keyboard/Escape/interactive minimization observation is not claimed;
existing input code is preserved and automated finite-frame exits pass. Resize
is covered by the retained M3 real-driver test, not by an interactive soak test.

State changes during an active Batch pass are a caller contract violation;
endFrame is the explicit flush boundary before switching state or renderer.
Queued textures must outlive flush. Consider typed render passes and texture
lifetime handles for M5, then instancing, fenced buffer rings, atlas policies,
and cross-vendor performance/pixel CI after measuring actual bottlenecks.

## M4.1 fixes and regression coverage

1. Texture2D::requireCurrentContext reuses GLObject ownership validation without
   GL binding changes. Batch checks both its own and texture context before any
   queue mutation or automatic flush. Tests reject an A texture in unshared B,
   preserve a full one-sprite queue/statistics/bindings/pixels, reject moved-from
   texture, and render again in A. RAII restores A before its resources die.
2. New M41GPUTests uses nine distinct RGBA textures, 22 overlapping submissions,
   repeated texture IDs before/after overflow, and two opposite orders. Each
   frame starts with explicit framebuffer, viewport, blend, clear, depth/scissor/
   cull and color-write state. Full RGBA readback matches Immediate exactly in
   both orders, and reversing the order changes the reference image. Three
   draws/flushes and eighteen actual binds per order agree with slot grouping.
3. Sandbox creates only the selected sprite renderer, and neither in triangle
   mode. Redundant beginFrame and minimized empty-pass GL work removed. Displayed
   counts reset every frame; internal stats describe the last begun pass.
4. Move assignment now may throw: any active source or target is rejected before
   mutation. Inactive owners require their creating context current before
   release/transfer. Move construction preserves active queue/state snapshots.
   Tests cover all four assignment states, construction, moved-from APIs,
   self-assignment and reuse after rejection. Existing deletion/failure injection
   tests remain, including buffer upload failure propagation and recovery.

Debug/Release builds and all 10 CTest tests pass in a real macOS GUI session.
No new GL entry points were added to production; existing failure-test doubles
remain sufficient. No correctness checks were removed for performance.
See [separate M4.1 performance report](performance-m41.md) for the complete rerun.

Remaining debt: queued texture lifetimes are caller-managed, not reference-counted;
active passes require exclusive GL state ownership; destruction still requires
the correct live Context. Cross-driver CI and external GPU allocation profiling
remain unperformed. Actual keyboard/Escape/minimization interaction is unverified;
finite-frame exits and zero-sprite counts are verified. No M5 features added.
