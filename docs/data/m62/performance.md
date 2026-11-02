# M6.2 CPU timing observations

30 warmups + 300 samples/workload, Release, real OpenGL. No GPU completion synchronization.
World workload omits demo grid (82 sprites). UI includes all HUD/overlay glyphs.

|Workload|Fixed p50 us|HUD p50 / max us|World p50 us|UI p50 us|Total submit p50 us|World/UI draws|World/UI objects|
|---|---:|---:|---:|---:|---:|---|---|
|Playing|0.125|0.250 / 20.875|10.917|31.729|43.124|1/1|1/225|
|48 enemies / 64 projectiles|22.292|0.250 / 20.042|14.521|22.105|36.687|1/1|113/225|
|Victory|0.000|0.250 / 0.791|5.042|74.812|80.104|1/1|1/863|
|GameOver|0.000|0.250 / 0.708|5.084|66.688|71.791|1/1|2/885|
|After restart|0.125|0.250 / 14.750|4.709|19.645|24.395|1/1|1/225|

HUD maxima include changed-second rebuilds in Playing; terminal samples use cached scenes. First-build cost is in warmup, not these distributions. Total submission excludes simulation, HUD preparation, clear and swap. Full engine O(N²) collision and rendering contracts remain unchanged.
