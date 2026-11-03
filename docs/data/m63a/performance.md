# M6.3a Release measurements

macOS 26.5.1 (25F80), arm64. Local Release build, real OpenGL context.
`tiny2d_m63a_benchmark` uses unmodified levels and constant movement (1,0).
`--stress` changes only playerMaxHealth to 100000 to keep the 90-second schedule active.
This is a profiling fixture, not a playable difficulty. Each row is one fixed update
opportunity, HUD update, world submission, UI submission, glFinish, menu update.
The world excludes the decorative grid. First 30 rows per level are excluded from
percentiles; peaks and checkpoints include every row. No wall clock drives gameplay.
Menu cost includes cached updates; the first row records construction.
`gpu_drain_us` is CPU wait in glFinish for outstanding GPU work, NOT a GPU timer-query
execution duration. Submission includes driver work but excludes explicit completion.
No isolated GPU execution-time claim is made.

## Full-duration stress (microseconds)

|Lv|Fixed p50/p95|HUD p50/p95|World submit p50/p95|UI submit p50/p95|GPU drain p50/p95|Enemy/projectile peak|World/UI draws max|
|---|---|---|---|---|---|---|---|
|1|1.583 / 4.083|0.500 / 0.667|18.208 / 20.708|45.166 / 48.125|175.500 / 260.917|35/3|1/2|
|2|1.917 / 6.083|0.459 / 0.667|18.208 / 20.458|42.958 / 45.750|169.833 / 243.250|48/3|1/2|
|3|2.541 / 7.625|0.459 / 0.667|18.417 / 20.958|45.792 / 49.292|173.666 / 248.375|48/3|1/2|
|4|3.042 / 8.000|0.500 / 0.833|19.542 / 24.375|42.167 / 49.541|176.375 / 283.542|48/4|1/2|
|5|4.125 / 8.250|0.459 / 55.458|19.083 / 21.584|45.833 / 50.625|173.417 / 252.166|48/4|1/2|

## Actual stress checkpoint counters

|Lv|30s spawned/kills|60s spawned/kills|90s spawned/kills|
|---|---|---|---|
|1|30/19|58/43|98/63|
|2|34/24|68/46|116/68|
|3|38/24|78/50|120/72|
|4|43/27|88/51|122/74|
|5|47/28|98/57|131/84|

## Unmodified levels

|Lv|Terminal tick|Spawned/kills|Active-only fixed p50/p95|
|---|---|---|---|
|1|3593|58/42|1.333/2.708|
|2|3868|74/49|1.500/3.250|
|3|1978|42/27|1.375/2.000|
|4|1761|42/26|1.334/2.083|
|5|1586|42/25|1.458/2.416|

Terminal rows are frozen. The 30/60/90 second wall-opportunity checkpoints in
performance.csv may already be terminal and must not be described as active simulation.

Cached menu p50/p95: 0.125/0.167 us. First construction by level: 531.208, 504.25, 524.083, 523, 503.375 us.

Existing M62 maximum-population fixture (48 enemies, 64 projectiles): fixed p50/p95 20.625/24.917 us. Historical M62 fixed p50 was 22.292 us; this is not a controlled hardware/load comparison. Renderer hot paths are unchanged and no broad-phase replacement is justified by these measurements.
