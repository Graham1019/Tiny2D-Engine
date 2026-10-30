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
