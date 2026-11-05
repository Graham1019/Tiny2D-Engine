# M6.3b.1 measured distributions

All timing values are microseconds; cells are p50 / p95 / p99.

## Lv.1

|Metric|Before 5478299|Review fix|
|---|---|---|
|fixed_us|0.667 / 1.834 / 3.125|0.667 / 1.709 / 3.083|
|scene_proxy_us|0.625 / 1.000 / 1.250|0.667 / 0.875 / 1.000|
|vfx_update_us|0.083 / 0.167 / 0.209|0.083 / 0.125 / 0.167|
|vfx_consume_us|0.042 / 0.125 / 0.208|0.042 / 0.125 / 0.208|
|vfx_present_us|0.917 / 1.334 / 1.500|1.084 / 1.417 / 1.500|
|hud_us|0.458 / 1.083 / 32.500|0.458 / 0.917 / 31.792|
|world_us|19.583 / 31.583 / 40.458|17.833 / 25.083 / 34.125|
|ui_us|31.458 / 33.416 / 35.583|30.542 / 32.166 / 35.166|
|frame_cpu_us|55.333 / 81.000 / 91.292|52.708 / 73.708 / 88.000|
|finish_wait_us|283.417 / 350.375 / 621.208|288.500 / 349.792 / 763.375|
|frame_completed_us|341.334 / 400.500 / 676.833|342.042 / 405.375 / 821.000|

CPU frame p95 change: -9.0%.

|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|Dropped cumulative max|
|---|---|---|---|---|---|---|---|---|
|before|1/1|382|2/2|55008|8/1|5|10|0|
|after|1/1|382|2/2|55008|8/1|5|10|0|

Completed offscreen throughput (1e6 / median frame_completed_us): 2923.6 frames/s. This excludes display swap/vsync and is not interactive FPS.

## Lv.3

|Metric|Before 5478299|Review fix|
|---|---|---|
|fixed_us|0.709 / 2.042 / 2.334|0.667 / 1.792 / 2.500|
|scene_proxy_us|0.708 / 0.917 / 1.083|0.708 / 0.833 / 0.917|
|vfx_update_us|0.083 / 0.167 / 0.209|0.083 / 0.125 / 0.166|
|vfx_consume_us|0.042 / 0.125 / 0.125|0.042 / 0.125 / 0.125|
|vfx_present_us|1.042 / 1.250 / 1.292|1.250 / 1.458 / 1.584|
|hud_us|0.500 / 0.708 / 38.791|0.500 / 0.625 / 37.292|
|world_us|20.625 / 24.334 / 25.333|20.167 / 22.750 / 24.708|
|ui_us|33.292 / 34.834 / 36.375|32.625 / 38.583 / 40.125|
|frame_cpu_us|58.459 / 64.083 / 89.792|57.542 / 69.166 / 87.416|
|finish_wait_us|290.334 / 349.834 / 499.000|292.042 / 331.917 / 527.375|
|frame_completed_us|348.875 / 417.500 / 558.750|351.583 / 390.458 / 591.208|

CPU frame p95 change: +7.9%.

|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|Dropped cumulative max|
|---|---|---|---|---|---|---|---|---|
|before|1/1|397|2/2|57168|9/1|5|11|0|
|after|1/1|397|2/2|57168|9/1|5|11|0|

Completed offscreen throughput (1e6 / median frame_completed_us): 2844.3 frames/s. This excludes display swap/vsync and is not interactive FPS.

## Lv.5

|Metric|Before 5478299|Review fix|
|---|---|---|
|fixed_us|0.792 / 1.583 / 2.667|0.791 / 1.667 / 2.417|
|scene_proxy_us|0.666 / 0.875 / 1.000|0.708 / 0.917 / 1.083|
|vfx_update_us|0.083 / 0.125 / 0.125|0.083 / 0.125 / 0.125|
|vfx_consume_us|0.042 / 0.084 / 0.125|0.042 / 0.084 / 0.167|
|vfx_present_us|1.083 / 1.250 / 1.292|1.292 / 1.459 / 1.542|
|hud_us|0.500 / 0.709 / 37.833|0.500 / 0.709 / 37.792|
|world_us|20.500 / 22.000 / 22.583|20.750 / 23.000 / 24.334|
|ui_us|33.958 / 35.500 / 36.834|33.459 / 35.916 / 38.917|
|frame_cpu_us|58.542 / 62.208 / 91.083|58.667 / 66.250 / 88.708|
|finish_wait_us|169.125 / 214.458 / 244.750|287.375 / 350.917 / 466.875|
|frame_completed_us|228.000 / 274.084 / 303.334|346.583 / 408.959 / 524.542|

CPU frame p95 change: +6.5%.

|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|Dropped cumulative max|
|---|---|---|---|---|---|---|---|---|
|before|1/1|413|2/2|59472|9/1|6|11|0|
|after|1/1|413|2/2|59472|9/1|6|11|0|

Completed offscreen throughput (1e6 / median frame_completed_us): 2885.3 frames/s. This excludes display swap/vsync and is not interactive FPS.

## 48 enemies + 64 projectiles

|Metric|Before 5478299|Review fix|
|---|---|---|
|fixed_us|23.250 / 24.958 / 25.500|23.500 / 24.666 / 25.584|
|scene_proxy_us|26.833 / 28.917 / 31.250|26.667 / 27.125 / 32.708|
|vfx_update_us|0.042 / 0.084 / 0.125|0.042 / 0.084 / 0.084|
|vfx_consume_us|0.250 / 0.333 / 0.458|5.459 / 5.917 / 6.125|
|vfx_present_us|1.709 / 1.833 / 1.959|1.875 / 2.083 / 2.500|
|hud_us|0.375 / 0.417 / 26.416|0.375 / 0.458 / 26.459|
|world_us|42.375 / 43.291 / 49.666|41.708 / 45.667 / 50.542|
|ui_us|28.625 / 29.417 / 30.708|27.959 / 32.916 / 35.292|
|frame_cpu_us|97.625 / 100.708 / 123.041|102.209 / 112.042 / 126.500|
|finish_wait_us|167.750 / 196.125 / 203.042|189.459 / 236.292 / 336.584|
|frame_completed_us|264.542 / 294.084 / 300.250|291.541 / 344.417 / 444.584|

CPU frame p95 change: +11.3%.

|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|Dropped cumulative max|
|---|---|---|---|---|---|---|---|---|
|before|1/1|770|2/2|110880|48/64|128|113|8192|
|after|1/1|770|2/2|110880|48/64|128|113|8192|

Completed offscreen throughput (1e6 / median frame_completed_us): 3430.0 frames/s. This excludes display swap/vsync and is not interactive FPS.

## 8 kills per tick

|Metric|Before 5478299|Review fix|
|---|---|---|
|fixed_us|7.125 / 7.583 / 9.958|7.083 / 7.542 / 7.917|
|scene_proxy_us|0.167 / 0.292 / 0.375|0.125 / 0.208 / 0.292|
|vfx_update_us|0.083 / 0.084 / 0.125|0.083 / 0.084 / 0.125|
|vfx_consume_us|0.083 / 0.125 / 0.125|2.250 / 2.416 / 2.500|
|vfx_present_us|2.500 / 2.625 / 2.625|2.834 / 3.334 / 3.375|
|hud_us|35.333 / 36.792 / 37.917|35.167 / 37.500 / 57.125|
|world_us|48.000 / 48.750 / 52.000|46.750 / 47.833 / 49.125|
|ui_us|30.583 / 32.000 / 38.000|29.542 / 30.583 / 31.000|
|frame_cpu_us|125.042 / 129.709 / 132.792|125.375 / 128.708 / 144.417|
|finish_wait_us|166.041 / 199.625 / 229.917|166.208 / 219.667 / 301.750|
|frame_completed_us|290.375 / 323.417 / 354.292|291.083 / 345.750 / 446.167|

CPU frame p95 change: -0.8%.

|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|Dropped cumulative max|
|---|---|---|---|---|---|---|---|---|
|before|1/1|911|2/2|131184|0/0|128|1|2992|
|after|1/1|911|2/2|131184|0/0|128|1|2992|

Completed offscreen throughput (1e6 / median frame_completed_us): 3435.4 frames/s. This excludes display swap/vsync and is not interactive FPS.

## Victory

|Metric|Before 5478299|Review fix|
|---|---|---|
|fixed_us|0.000 / 0.042 / 0.042|0.000 / 0.042 / 0.042|
|scene_proxy_us|0.167 / 0.291 / 0.292|0.125 / 0.167 / 0.250|
|vfx_update_us|0.042 / 0.084 / 0.167|0.042 / 0.083 / 0.084|
|vfx_consume_us|0.000 / 0.042 / 0.042|0.041 / 0.042 / 0.042|
|vfx_present_us|0.958 / 1.166 / 1.167|1.166 / 1.375 / 1.375|
|hud_us|0.500 / 0.667 / 0.750|0.500 / 0.666 / 0.709|
|world_us|17.958 / 18.667 / 19.292|17.792 / 18.416 / 19.500|
|ui_us|80.125 / 82.208 / 90.209|78.084 / 80.291 / 88.292|
|frame_cpu_us|101.125 / 103.459 / 110.125|98.875 / 101.208 / 109.500|
|finish_wait_us|162.583 / 195.333 / 207.959|164.292 / 184.250 / 227.000|
|frame_completed_us|264.125 / 295.917 / 308.292|263.333 / 284.500 / 325.833|

CPU frame p95 change: -2.2%.

|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|Dropped cumulative max|
|---|---|---|---|---|---|---|---|---|
|before|1/1|990|2/2|142560|0/0|0|1|0|
|after|1/1|990|2/2|142560|0/0|0|1|0|

Completed offscreen throughput (1e6 / median frame_completed_us): 3797.5 frames/s. This excludes display swap/vsync and is not interactive FPS.

## GameOver

|Metric|Before 5478299|Review fix|
|---|---|---|
|fixed_us|0.000 / 0.042 / 0.042|0.000 / 0.042 / 0.042|
|scene_proxy_us|0.292 / 0.417 / 0.583|0.292 / 0.417 / 0.500|
|vfx_update_us|0.042 / 0.084 / 0.084|0.042 / 0.083 / 0.084|
|vfx_consume_us|0.000 / 0.042 / 0.042|0.000 / 0.042 / 0.042|
|vfx_present_us|1.000 / 1.125 / 1.209|1.167 / 1.375 / 1.375|
|hud_us|0.500 / 0.667 / 0.709|0.500 / 0.667 / 0.708|
|world_us|18.166 / 18.625 / 18.750|18.042 / 18.417 / 19.292|
|ui_us|83.250 / 87.083 / 90.875|81.542 / 88.209 / 91.417|
|frame_cpu_us|104.208 / 108.416 / 111.833|102.500 / 110.042 / 112.166|
|finish_wait_us|164.709 / 207.167 / 226.750|162.917 / 182.084 / 204.875|
|frame_completed_us|267.292 / 311.542 / 331.291|265.084 / 289.542 / 307.292|

CPU frame p95 change: +1.5%.

|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|Dropped cumulative max|
|---|---|---|---|---|---|---|---|---|
|before|1/2|1013|3/3|145872|1/0|0|2|0|
|after|1/2|1013|3/3|145872|1/0|0|2|0|

Completed offscreen throughput (1e6 / median frame_completed_us): 3772.4 frames/s. This excludes display swap/vsync and is not interactive FPS.

## Restart

|Metric|Before 5478299|Review fix|
|---|---|---|
|fixed_us|0.125 / 0.167 / 0.250|0.125 / 0.167 / 0.209|
|scene_proxy_us|0.167 / 0.292 / 0.417|0.125 / 0.167 / 0.250|
|vfx_update_us|0.042 / 0.084 / 0.084|0.042 / 0.084 / 0.084|
|vfx_consume_us|0.000 / 0.042 / 0.042|0.041 / 0.042 / 0.042|
|vfx_present_us|0.959 / 1.167 / 1.250|1.167 / 1.334 / 1.417|
|hud_us|0.459 / 0.625 / 33.375|0.459 / 0.625 / 34.792|
|world_us|17.958 / 19.208 / 21.792|17.667 / 18.833 / 20.500|
|ui_us|33.708 / 36.375 / 39.000|33.583 / 36.000 / 38.583|
|frame_cpu_us|54.666 / 59.500 / 77.542|54.500 / 59.084 / 78.750|
|finish_wait_us|168.708 / 213.833 / 220.625|165.625 / 206.125 / 249.583|
|frame_completed_us|224.083 / 267.750 / 275.291|221.708 / 265.209 / 304.708|

CPU frame p95 change: -0.7%.

|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|Dropped cumulative max|
|---|---|---|---|---|---|---|---|---|
|before|1/1|354|2/2|50976|0/0|0|1|0|
|after|1/1|354|2/2|50976|0/0|0|1|0|

Completed offscreen throughput (1e6 / median frame_completed_us): 4510.4 frames/s. This excludes display swap/vsync and is not interactive FPS.

## Menu

|Metric|Before 5478299|Review fix|
|---|---|---|
|fixed_us|0.000 / 0.042 / 0.042|0.000 / 0.042 / 0.042|
|scene_proxy_us|0.125 / 0.250 / 0.292|0.125 / 0.208 / 0.209|
|vfx_update_us|0.000 / 0.042 / 0.042|0.000 / 0.042 / 0.042|
|vfx_consume_us|0.000 / 0.042 / 0.042|0.000 / 0.042 / 0.042|
|vfx_present_us|0.000 / 0.042 / 0.042|0.000 / 0.042 / 0.042|
|hud_us|0.042 / 0.125 / 0.208|0.042 / 0.084 / 0.125|
|world_us|0.000 / 0.042 / 0.042|0.000 / 0.042 / 0.042|
|ui_us|171.208 / 174.583 / 194.333|165.709 / 171.042 / 179.500|
|frame_cpu_us|172.666 / 175.625 / 195.792|167.250 / 173.167 / 181.708|
|finish_wait_us|179.375 / 216.208 / 236.000|180.417 / 211.542 / 228.375|
|frame_completed_us|352.750 / 388.291 / 415.333|347.459 / 379.542 / 396.084|

CPU frame p95 change: -1.4%.

|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|Dropped cumulative max|
|---|---|---|---|---|---|---|---|---|
|before|0/3|2009|3/3|289296|0/0|0|1|0|
|after|0/3|2009|3/3|289296|0/0|0|1|0|

Completed offscreen throughput (1e6 / median frame_completed_us): 2878.0 frames/s. This excludes display swap/vsync and is not interactive FPS.

## Saturated trails

|Metric|Before 5478299|Review fix|
|---|---|---|
|fixed_us|9.958 / 10.125 / 10.333|9.916 / 10.125 / 10.500|
|scene_proxy_us|9.750 / 10.000 / 10.084|9.791 / 10.083 / 10.208|
|vfx_update_us|0.083 / 0.084 / 0.125|0.083 / 0.167 / 0.167|
|vfx_consume_us|0.250 / 0.292 / 0.334|7.042 / 7.250 / 7.459|
|vfx_present_us|1.958 / 2.042 / 2.083|2.125 / 2.250 / 2.375|
|hud_us|0.459 / 0.542 / 30.542|0.458 / 0.542 / 27.958|
|world_us|38.958 / 40.417 / 41.750|37.041 / 39.708 / 45.208|
|ui_us|28.583 / 29.250 / 29.417|28.000 / 28.916 / 32.375|
|frame_cpu_us|81.208 / 83.292 / 107.083|85.625 / 91.750 / 110.125|
|finish_wait_us|162.167 / 188.542 / 214.875|165.333 / 182.542 / 198.833|
|frame_completed_us|243.375 / 268.709 / 295.083|250.791 / 268.416 / 281.833|

CPU frame p95 change: +10.2%.

|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|Dropped cumulative max|
|---|---|---|---|---|---|---|---|---|
|before|1/1|674|2/2|97056|0/64|128|65|8192|
|after|1/1|674|2/2|97056|0/64|128|65|8192|

Completed offscreen throughput (1e6 / median frame_completed_us): 3987.4 frames/s. This excludes display swap/vsync and is not interactive FPS.

## Restart first frame

|Metric|Before 5478299|Review fix|
|---|---|---|
|fixed_us|0.083 / 0.125 / 0.125|0.125 / 0.166 / 0.167|
|scene_proxy_us|0.042 / 0.084 / 0.125|0.042 / 0.125 / 0.125|
|vfx_update_us|0.042 / 0.083 / 0.084|0.042 / 0.083 / 0.084|
|vfx_consume_us|0.875 / 0.917 / 1.084|0.875 / 0.917 / 1.125|
|vfx_present_us|0.667 / 0.709 / 0.750|0.750 / 0.791 / 1.000|
|hud_us|0.375 / 0.375 / 0.500|0.334 / 0.375 / 0.375|
|world_us|13.500 / 14.042 / 18.125|13.250 / 14.250 / 15.917|
|ui_us|29.125 / 29.667 / 36.000|28.667 / 29.084 / 37.708|
|frame_cpu_us|45.250 / 46.334 / 57.583|44.833 / 51.042 / 55.041|
|finish_wait_us|182.959 / 226.625 / 279.833|181.041 / 224.375 / 286.833|
|frame_completed_us|228.083 / 271.667 / 325.041|226.958 / 269.375 / 331.541|

CPU frame p95 change: +10.2%.

|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|Dropped cumulative max|
|---|---|---|---|---|---|---|---|---|
|before|1/1|354|2/2|50976|0/0|0|1|0|
|after|1/1|354|2/2|50976|0/0|0|1|0|

Completed offscreen throughput (1e6 / median frame_completed_us): 4406.1 frames/s. This excludes display swap/vsync and is not interactive FPS.
