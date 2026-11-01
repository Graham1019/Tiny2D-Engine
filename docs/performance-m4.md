# M4 measured performance

Measured on 2026-10-30 (Asia/Shanghai): Apple M5 Pro CPU/GPU, macOS 26.5.1
(25F80), AppleClang 21.0.0.21000101, CMake Release, OpenGL 4.1 Metal - 90.5.
The context requests 3.3 Core; shaders use GLSL 330. GL_ARB_timer_query is
available. All 9600 measured GPU queries completed; no unavailable values were
substituted. Driver identification is in [environment.txt](data/m4/environment.txt).

## Method

One 800x600 logical window with 1600x1200 framebuffer, identical white RGBA8 texture, deterministic grid,
straight-alpha tint, camera and transforms in both renderers. CPU workload uses
2x2 logical-pixel sprites, emphasizing submission; pixel workload uses 110x110
sprites and substantial overlapping fill. Grid wraps after 7500 sprites; some
large quads cross the window edge. Neither workload is a pure isolated bottleneck.
Default batch capacity 1000, eight slots, one texture used. VSync is disabled only
for the benchmark. Each renderer/count/workload combination receives 30 warmup
frames followed by 120 measured frames, repeated five times. Renderer execution
order alternates across repeats. Final run was separate from GPU test execution.

CPU steady_clock interval includes beginFrame, transformations, submission,
flush and endFrame state restoration, excluding clear, swap, events, file output
and query retrieval. Immediate retains public safety checks, and Batch includes
state save/restore. GPU elapsed queries surround that submission interval; the
GPU timeline can include CPU starvation/driver scheduling, not just shader cost.
Sixteen query objects are reused only after RESULT_AVAILABLE. Busy slots skip
sampling instead of blocking. Trial-end glFinish drains remaining samples outside
the CPU timing interval; no immediate per-frame query wait is used.

Percentiles sort samples and select floor((N-1)*q). The table shows the **median
of the five trial p50/p95 values**, in microseconds; full per-trial results and
individual frame values are linked below. No process-wall-clock timing or FPS
ratio is derived from these numbers.

| Workload | Sprites | Renderer | CPU p50 µs | CPU p95 µs | GPU p50 µs | GPU p95 µs | Draws | Flushes | Binds | Upload bytes |
|---|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|
| cpu | 100 | immediate | 132.79 | 149.96 | 102.83 | 212.83 | 100 | 0 | 100 | 0 |
| cpu | 100 | batch | 73.29 | 87.83 | 145.88 | 205.79 | 1 | 1 | 1 | 14400 |
| cpu | 500 | immediate | 493.67 | 512.54 | 240.25 | 279.50 | 500 | 0 | 500 | 0 |
| cpu | 500 | batch | 88.04 | 107.54 | 166.38 | 222.46 | 1 | 1 | 1 | 72000 |
| cpu | 1000 | immediate | 877.38 | 916.79 | 347.50 | 396.00 | 1000 | 0 | 1000 | 0 |
| cpu | 1000 | batch | 106.21 | 120.62 | 178.62 | 234.12 | 1 | 1 | 1 | 144000 |
| cpu | 5000 | immediate | 4211.29 | 4334.08 | 2823.88 | 2916.54 | 5000 | 0 | 5000 | 0 |
| cpu | 5000 | batch | 271.50 | 294.79 | 150.38 | 167.00 | 5 | 5 | 5 | 720000 |
| pixel | 100 | immediate | 139.17 | 151.67 | 282.88 | 422.79 | 100 | 0 | 100 | 0 |
| pixel | 100 | batch | 80.29 | 121.08 | 550.13 | 678.92 | 1 | 1 | 1 | 14400 |
| pixel | 500 | immediate | 527.67 | 609.08 | 814.50 | 1203.21 | 500 | 0 | 500 | 0 |
| pixel | 500 | batch | 95.00 | 121.96 | 518.25 | 568.00 | 1 | 1 | 1 | 72000 |
| pixel | 1000 | immediate | 979.17 | 1027.04 | 554.88 | 584.21 | 1000 | 0 | 1000 | 0 |
| pixel | 1000 | batch | 137.71 | 200.88 | 932.83 | 976.87 | 1 | 1 | 1 | 144000 |
| pixel | 5000 | immediate | 4402.17 | 6347.83 | 4429.75 | 7304.37 | 5000 | 0 | 5000 | 0 |
| pixel | 5000 | batch | 271.04 | 362.33 | 3748.42 | 3924.00 | 5 | 5 | 5 | 720000 |

## Raw data and limits

- [All 80 trial summaries](data/m4/trials.csv)
- [9600 CPU frame samples](data/m4/m4-frames.csv)
- [9600 GPU frame samples](data/m4/m4-gpu-frames.csv)
- [Debug CTest](data/m4/debug-ctest.txt), [Release CTest](data/m4/release-ctest.txt), [pixel regression](data/m4/pixel.txt)

Batch reduces actual draw/bind counts and CPU submission time at larger counts.
The single-texture 500-sprite case measures 500 versus 1 draw, but uploads 72000
vertex bytes versus the immediate renderer's immutable quad (zero frame upload).
This CPU-work / bandwidth tradeoff is explicit. GPU improvements vary by workload
and count; fewer calls do not imply the same reduction in GPU elapsed time or FPS.

Only one machine, driver and measurement session are represented. Thermal state,
background OS activity, compositor scheduling, Retina framebuffer scaling and
query overhead affect variance. Five repeats do not establish confidence intervals
or universal performance. Raw outliers are retained. Small CPU sprites still
have GPU cost; large sprites still have CPU cost. An end-to-end application FPS
claim would require a separate controlled test including presentation and updates.
M3's 120-frame process-wall-clock results remain historical reference only.

Run `python3 scripts/summarize_m4.py docs/data/m4/trials.csv` to regenerate
the aggregate values.

Reproduce with commands in [milestone-04.md](milestone-04.md); run Release in a
logged-in macOS GUI session and archive each output directory separately.
