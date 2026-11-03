# M6.3b measured distributions

All timing values are microseconds; cells are p50 / p95 / p99.

## Lv.1

|Metric|M6.3a|M6.3b|
|---|---|---|
|fixed_us|0.709 / 1.792 / 3.875|0.875 / 1.792 / 2.916|
|scene_proxy_us|0.709 / 0.917 / 1.042|0.750 / 1.000 / 1.209|
|vfx_update_us|0.000 / 0.042 / 0.042|0.125 / 0.250 / 0.292|
|vfx_present_us|0.000 / 0.042 / 0.042|1.125 / 1.250 / 1.291|
|hud_us|0.500 / 0.708 / 42.917|0.541 / 0.708 / 36.917|
|world_us|17.917 / 24.000 / 29.000|21.834 / 26.459 / 32.666|
|ui_us|33.125 / 36.750 / 37.666|31.667 / 33.083 / 33.791|
|frame_cpu_us|55.250 / 63.958 / 90.041|57.625 / 70.583 / 93.000|
|finish_wait_us|300.083 / 631.125 / 668.750|284.750 / 333.875 / 378.125|
|frame_completed_us|355.791 / 686.458 / 724.875|346.000 / 392.958 / 434.875|

CPU frame p95 change: +10.4%.

|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|
|---|---|---|---|---|---|---|---|
|baseline|1/1|362|2/2|52128|8/1|0|10|
|polish|1/1|382|2/2|55008|8/1|5|10|

Completed offscreen throughput (1e6 / median frame_completed_us): 2890.2 frames/s. This excludes display swap/vsync and is not interactive FPS.

## Lv.3

|Metric|M6.3a|M6.3b|
|---|---|---|
|fixed_us|0.750 / 1.792 / 2.875|0.833 / 1.916 / 2.583|
|scene_proxy_us|0.666 / 0.792 / 0.875|0.666 / 0.916 / 1.042|
|vfx_update_us|0.000 / 0.042 / 0.042|0.125 / 0.208 / 0.250|
|vfx_present_us|0.000 / 0.042 / 0.042|1.084 / 1.250 / 1.292|
|hud_us|0.500 / 0.667 / 37.250|0.459 / 0.625 / 38.875|
|world_us|17.375 / 21.250 / 28.958|20.875 / 22.792 / 24.042|
|ui_us|34.334 / 38.625 / 46.709|33.083 / 35.417 / 39.375|
|frame_cpu_us|54.667 / 68.375 / 81.792|58.292 / 65.500 / 89.084|
|finish_wait_us|214.875 / 317.834 / 351.000|193.084 / 213.917 / 254.500|
|frame_completed_us|272.041 / 375.459 / 404.458|252.375 / 278.209 / 313.166|

CPU frame p95 change: -4.2%.

|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|
|---|---|---|---|---|---|---|---|
|baseline|1/1|377|2/2|54288|9/1|0|11|
|polish|1/1|397|2/2|57168|9/1|5|11|

Completed offscreen throughput (1e6 / median frame_completed_us): 3962.4 frames/s. This excludes display swap/vsync and is not interactive FPS.

## Lv.5

|Metric|M6.3a|M6.3b|
|---|---|---|
|fixed_us|0.792 / 1.500 / 2.542|0.875 / 1.500 / 2.417|
|scene_proxy_us|0.666 / 0.833 / 0.875|0.666 / 0.833 / 0.875|
|vfx_update_us|0.000 / 0.042 / 0.042|0.125 / 0.208 / 0.250|
|vfx_present_us|0.000 / 0.042 / 0.042|1.084 / 1.208 / 1.291|
|hud_us|0.459 / 0.667 / 38.250|0.458 / 0.583 / 38.542|
|world_us|17.167 / 18.167 / 19.958|20.625 / 21.500 / 21.708|
|ui_us|35.333 / 36.792 / 39.000|34.125 / 36.125 / 40.250|
|frame_cpu_us|55.500 / 60.542 / 84.708|58.916 / 67.167 / 88.917|
|finish_wait_us|193.083 / 235.333 / 351.333|184.708 / 252.875 / 335.416|
|frame_completed_us|248.334 / 289.708 / 411.875|244.542 / 309.125 / 392.791|

CPU frame p95 change: +10.9%.

|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|
|---|---|---|---|---|---|---|---|
|baseline|1/1|394|2/2|56736|9/1|0|11|
|polish|1/1|413|2/2|59472|9/1|6|11|

Completed offscreen throughput (1e6 / median frame_completed_us): 4089.3 frames/s. This excludes display swap/vsync and is not interactive FPS.

## 48 enemies + 64 projectiles

|Metric|M6.3a|M6.3b|
|---|---|---|
|fixed_us|23.209 / 24.792 / 26.666|23.292 / 24.625 / 24.959|
|scene_proxy_us|26.625 / 27.042 / 27.083|26.708 / 27.167 / 28.167|
|vfx_update_us|0.000 / 0.042 / 0.042|0.292 / 0.375 / 0.583|
|vfx_present_us|0.000 / 0.042 / 0.042|1.667 / 1.792 / 2.125|
|hud_us|0.333 / 0.375 / 26.583|0.333 / 0.417 / 25.666|
|world_us|19.625 / 20.292 / 22.084|41.750 / 43.083 / 51.625|
|ui_us|28.292 / 28.875 / 33.167|28.083 / 29.166 / 33.959|
|frame_cpu_us|72.292 / 75.625 / 98.583|96.458 / 103.291 / 122.375|
|finish_wait_us|165.042 / 192.667 / 212.000|166.958 / 206.625 / 268.500|
|frame_completed_us|237.625 / 264.750 / 284.583|263.375 / 303.333 / 363.083|

CPU frame p95 change: +36.6%.

|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|
|---|---|---|---|---|---|---|---|
|baseline|1/1|465|2/2|66960|48/64|0|113|
|polish|1/1|770|2/2|110880|48/64|128|113|

Completed offscreen throughput (1e6 / median frame_completed_us): 3796.9 frames/s. This excludes display swap/vsync and is not interactive FPS.

## 8 kills per tick

|Metric|M6.3a|M6.3b|
|---|---|---|
|fixed_us|7.250 / 8.167 / 14.042|7.083 / 8.334 / 11.334|
|scene_proxy_us|0.125 / 0.250 / 0.292|0.125 / 0.250 / 0.292|
|vfx_update_us|0.000 / 0.042 / 0.042|0.167 / 0.208 / 0.250|
|vfx_present_us|0.000 / 0.042 / 0.042|2.542 / 2.584 / 2.625|
|hud_us|35.750 / 37.334 / 38.500|35.458 / 36.833 / 38.333|
|world_us|12.125 / 13.208 / 16.084|47.250 / 53.042 / 60.333|
|ui_us|29.875 / 31.166 / 34.834|29.833 / 31.750 / 37.125|
|frame_cpu_us|86.209 / 91.541 / 92.791|123.875 / 130.542 / 142.208|
|finish_wait_us|166.958 / 192.709 / 229.625|162.459 / 198.583 / 209.458|
|frame_completed_us|252.875 / 279.875 / 315.250|287.083 / 323.208 / 358.833|

CPU frame p95 change: +42.6%.

|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|
|---|---|---|---|---|---|---|---|
|baseline|1/1|398|2/2|57312|0/0|0|1|
|polish|1/1|911|2/2|131184|0/0|128|1|

Completed offscreen throughput (1e6 / median frame_completed_us): 3483.3 frames/s. This excludes display swap/vsync and is not interactive FPS.

## Victory

|Metric|M6.3a|M6.3b|
|---|---|---|
|fixed_us|0.000 / 0.042 / 0.042|0.000 / 0.042 / 0.042|
|scene_proxy_us|0.167 / 0.250 / 0.291|0.125 / 0.209 / 0.291|
|vfx_update_us|0.000 / 0.042 / 0.042|0.042 / 0.084 / 0.166|
|vfx_present_us|0.000 / 0.042 / 0.042|0.958 / 1.125 / 1.167|
|hud_us|0.458 / 0.583 / 0.625|0.459 / 0.625 / 0.667|
|world_us|16.125 / 17.000 / 18.583|18.167 / 18.834 / 19.208|
|ui_us|79.000 / 80.750 / 83.916|78.375 / 79.750 / 86.667|
|frame_cpu_us|96.750 / 99.334 / 101.667|99.250 / 101.250 / 109.125|
|finish_wait_us|161.125 / 180.958 / 209.875|162.417 / 178.666 / 213.875|
|frame_completed_us|257.834 / 277.916 / 308.291|261.750 / 285.291 / 312.541|

CPU frame p95 change: +1.9%.

|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|
|---|---|---|---|---|---|---|---|
|baseline|1/1|988|2/2|142272|0/0|0|1|
|polish|1/1|990|2/2|142560|0/0|0|1|

Completed offscreen throughput (1e6 / median frame_completed_us): 3820.4 frames/s. This excludes display swap/vsync and is not interactive FPS.

## GameOver

|Metric|M6.3a|M6.3b|
|---|---|---|
|fixed_us|0.000 / 0.042 / 0.042|0.000 / 0.042 / 0.042|
|scene_proxy_us|0.292 / 0.375 / 0.500|0.292 / 0.375 / 0.417|
|vfx_update_us|0.000 / 0.042 / 0.042|0.042 / 0.083 / 0.084|
|vfx_present_us|0.000 / 0.042 / 0.042|0.959 / 1.125 / 1.167|
|hud_us|0.458 / 0.583 / 0.625|0.500 / 0.625 / 0.667|
|world_us|16.250 / 16.792 / 17.041|18.334 / 18.958 / 19.166|
|ui_us|82.875 / 85.625 / 92.917|81.917 / 83.750 / 87.791|
|frame_cpu_us|100.750 / 104.042 / 110.583|103.042 / 105.084 / 109.583|
|finish_wait_us|163.334 / 200.125 / 217.791|163.833 / 199.958 / 257.708|
|frame_completed_us|264.042 / 300.125 / 318.666|266.583 / 303.334 / 358.792|

CPU frame p95 change: +1.0%.

|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|
|---|---|---|---|---|---|---|---|
|baseline|1/2|1010|3/3|145440|1/0|0|2|
|polish|1/2|1013|3/3|145872|1/0|0|2|

Completed offscreen throughput (1e6 / median frame_completed_us): 3751.2 frames/s. This excludes display swap/vsync and is not interactive FPS.

## Restart

|Metric|M6.3a|M6.3b|
|---|---|---|
|fixed_us|0.125 / 0.208 / 0.250|0.125 / 0.167 / 0.250|
|scene_proxy_us|0.208 / 0.292 / 0.375|0.125 / 0.209 / 0.292|
|vfx_update_us|0.000 / 0.042 / 0.042|0.083 / 0.084 / 0.167|
|vfx_present_us|0.000 / 0.042 / 0.042|0.958 / 1.125 / 1.166|
|hud_us|0.458 / 0.583 / 34.958|0.417 / 0.583 / 34.584|
|world_us|16.042 / 17.000 / 18.041|18.083 / 18.500 / 18.959|
|ui_us|34.417 / 36.125 / 39.250|33.709 / 34.792 / 36.417|
|frame_cpu_us|52.709 / 56.166 / 78.166|54.625 / 57.125 / 79.083|
|finish_wait_us|164.875 / 205.459 / 219.417|170.209 / 213.708 / 233.834|
|frame_completed_us|218.042 / 255.792 / 272.333|224.917 / 267.416 / 288.792|

CPU frame p95 change: +1.7%.

|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|
|---|---|---|---|---|---|---|---|
|baseline|1/1|353|2/2|50832|0/0|0|1|
|polish|1/1|354|2/2|50976|0/0|0|1|

Completed offscreen throughput (1e6 / median frame_completed_us): 4446.1 frames/s. This excludes display swap/vsync and is not interactive FPS.

## Menu

|Metric|M6.3a|M6.3b|
|---|---|---|
|fixed_us|0.000 / 0.042 / 0.042|0.000 / 0.042 / 0.042|
|scene_proxy_us|0.209 / 0.834 / 1.375|0.125 / 0.167 / 0.208|
|vfx_update_us|0.000 / 0.042 / 0.042|0.000 / 0.042 / 0.042|
|vfx_present_us|0.000 / 0.042 / 0.042|0.000 / 0.042 / 0.042|
|hud_us|0.042 / 0.458 / 0.708|0.083 / 0.166 / 0.291|
|world_us|0.000 / 0.042 / 0.042|0.000 / 0.042 / 0.042|
|ui_us|167.041 / 244.916 / 302.167|165.458 / 168.417 / 177.792|
|frame_cpu_us|168.625 / 253.250 / 315.000|167.042 / 170.500 / 180.000|
|finish_wait_us|178.042 / 277.917 / 922.416|180.625 / 222.375 / 231.250|
|frame_completed_us|347.417 / 562.542 / 1173.290|347.750 / 389.000 / 399.708|

CPU frame p95 change: -32.7%.

|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|
|---|---|---|---|---|---|---|---|
|baseline|0/3|2009|3/3|289296|0/0|0|1|
|polish|0/3|2009|3/3|289296|0/0|0|1|

Completed offscreen throughput (1e6 / median frame_completed_us): 2875.6 frames/s. This excludes display swap/vsync and is not interactive FPS.
