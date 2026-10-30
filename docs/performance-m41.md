# M4.1 performance regression

M4.1 rerun on the same Apple M5 Pro, macOS 26.5.1 (25F80), AppleClang 21,
Release, OpenGL 4.1 Metal - 90.5. Logical window 800x600, framebuffer 1600x1200,
capacity 1000, slots 8, VSync off. Benchmark source/configuration is unchanged.
Two workloads, 100/500/1000/5000 sprites, 30 warmup + 120 measured frames per
trial, five repeats, alternating renderer order. Regression tests and Sandbox
finished before benchmark began. All 9600 CPU and 9600 GPU samples recorded;
timer queries available, deferred retrieval with the original 16-query ring.

Historical data in data/m4 is unchanged. New [trial summaries](data/m41/trials.csv),
[CPU frames](data/m41/m4-frames.csv), [GPU frames](data/m41/m4-gpu-frames.csv),
and [environment](data/m41/environment.txt) are separate files. Percentiles use
the original floor((N-1)*q) method; below are medians of five trial percentiles.
All times are microseconds. H = historical M4, N = new M4.1.

| Workload | N sprites | Renderer | CPU p50 H → N | CPU p95 H → N | GPU p50 H → N | GPU p95 H → N | Draw/flush/bind/upload bytes N |
|---|---:|---|---|---|---|---|---|
| cpu | 100 | immediate | 132.79 → 136.17 | 149.96 → 148.62 | 102.83 → 101.54 | 212.83 → 224.42 | 100/0/100/0 |
| cpu | 100 | batch | 73.29 → 74.54 | 87.83 → 89.96 | 145.88 → 150.38 | 205.79 → 207.42 | 1/1/1/14400 |
| cpu | 500 | immediate | 493.67 → 489.92 | 512.54 → 508.62 | 240.25 → 242.00 | 279.50 → 297.08 | 500/0/500/0 |
| cpu | 500 | batch | 88.04 → 90.67 | 107.54 → 108.62 | 166.38 → 166.75 | 222.46 → 222.38 | 1/1/1/72000 |
| cpu | 1000 | immediate | 877.38 → 882.71 | 916.79 → 918.04 | 347.50 → 345.50 | 396.00 → 381.92 | 1000/0/1000/0 |
| cpu | 1000 | batch | 106.21 → 108.62 | 120.62 → 123.67 | 178.62 → 171.04 | 234.12 → 240.75 | 1/1/1/144000 |
| cpu | 5000 | immediate | 4211.29 → 4241.04 | 4334.08 → 4381.96 | 2823.88 → 2836.92 | 2916.54 → 2936.00 | 5000/0/5000/0 |
| cpu | 5000 | batch | 271.50 → 280.88 | 294.79 → 305.92 | 150.38 → 150.00 | 167.00 → 166.62 | 5/5/5/720000 |
| pixel | 100 | immediate | 139.17 → 136.54 | 151.67 → 151.08 | 282.88 → 279.33 | 422.79 → 400.17 | 100/0/100/0 |
| pixel | 100 | batch | 80.29 → 75.88 | 121.08 → 93.21 | 550.13 → 524.38 | 678.92 → 595.38 | 1/1/1/14400 |
| pixel | 500 | immediate | 527.67 → 511.79 | 609.08 → 530.46 | 814.50 → 325.71 | 1203.21 → 348.38 | 500/0/500/0 |
| pixel | 500 | batch | 95.00 → 94.54 | 121.96 → 112.58 | 518.25 → 516.42 | 568.00 → 558.46 | 1/1/1/72000 |
| pixel | 1000 | immediate | 979.17 → 990.25 | 1027.04 → 1077.29 | 554.88 → 555.04 | 584.21 → 583.62 | 1000/0/1000/0 |
| pixel | 1000 | batch | 137.71 → 150.88 | 200.88 → 209.50 | 932.83 → 930.88 | 976.87 → 983.67 | 1/1/1/144000 |
| pixel | 5000 | immediate | 4402.17 → 4518.58 | 6347.83 → 4715.42 | 4429.75 → 3899.21 | 7304.37 → 4624.04 | 5000/0/5000/0 |
| pixel | 5000 | batch | 271.04 → 279.42 | 362.33 → 498.79 | 3748.42 → 3755.67 | 3924.00 → 3954.50 | 5/5/5/720000 |

## Interpretation and limits

For 500 CPU-workload sprites, Immediate CPU p50 changes 493.67 → 489.92 µs
(-0.8%); Batch 88.04 → 90.67 µs (+3.0%, +2.63 µs per frame). Batch CPU-workload
p50 at 5000 sprites changes 271.50 → 280.88 µs (+3.5%). Each accepted Batch draw
now validates both renderer and texture owners through GLObject::requireCurrent,
including main-thread/current-context checks. A modest submission cost increase
is consistent with those additional checks; single-session timing cannot isolate
that cost from OS/driver noise. Correctness checks were retained in Release.

The 500-sprite draw result is unchanged: 500 versus 1. Existing GPU tests verify
1000/1001 capacity boundaries and 5000-sprite statistics. New cross-slot tests
verify pixel correctness and actual flush/bind counts across nine textures.

Pixel workload results are less stable: Batch 1000 CPU p50 increases about 9.6%,
and Batch 5000 CPU p95 increases from 362.33 to 498.79 µs (about 37.7%), while
its GPU p95 changes only 3924.00 → 3954.50 µs (about 0.8%). Conversely Immediate
500 pixel GPU p50 drops 814.50 → 325.71 µs without an Immediate code change.
These changes show substantial session/driver scheduling variation. The measured
CPU submission interval includes driver backpressure, not only CPU arithmetic;
a fixed pair of context checks cannot by itself explain asymmetric tail spikes
or unchanged-path GPU shifts. Exact causes have not been isolated with a GPU
profiler. Raw outliers remain present, and no claim of zero performance regression
or FPS improvement is made. Repeat sessions are needed to establish tail-latency
confidence; this remains measurement debt, not a reason to remove validation.

## Reproduce

Use the original build and benchmark commands in milestone-04.md. Choose a new
output directory, e.g. `mkdir -p /tmp/tiny2d-m41-run`, then run the absolute path
to build/m4-release/tiny2d_benchmark there. Do not overwrite historical CSVs.
Aggregate with `python3 scripts/summarize_m4.py docs/data/m41/trials.csv`.
Debug/Release [CTest logs](data/m41/debug-ctest.txt) and
[Release CTest log](data/m41/release-ctest.txt) show 10/10 passed. The new
[pixel log](data/m41/pixel-release.txt) records both orders at max RGBA error 0,
3 draws/flushes, and 18 texture binds.
