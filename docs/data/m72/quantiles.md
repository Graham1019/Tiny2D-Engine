# M7.2 Release measurements

Nearest-rank quantiles by originating frame. CPU and GPU times use microseconds.

## 1600×900 · configuration 1

Inspector off, previews off, GPU timing off. Measurement GPU states {'off': 1200}. Pass mask 63.

| CPU metric | n | p50 | p95 | p99 |
|---|---:|---:|---:|---:|
| total_us | 1200 | 109.208 | 130.875 | 158.542 |
| world_us | 1200 | 30.208 | 40.792 | 59.125 |
| bright_us | 1200 | 7.750 | 12.250 | 14.458 |
| horizontal_us | 1200 | 6.917 | 11.375 | 13.708 |
| vertical_us | 1200 | 6.417 | 11.125 | 13.750 |
| output_us | 1200 | 9.333 | 14.416 | 17.125 |
| hud_us | 1200 | 35.375 | 44.250 | 47.708 |
| debug_us | 0 | — | — | — |
| preview_us | 0 | — | — | — |
| gui_us | 0 | — | — | — |
| update_us | 1200 | 0.042 | 0.167 | 0.500 |
| prepare_us | 1200 | 7.792 | 12.083 | 14.625 |
| gui_build_us | 0 | — | — | — |
| query_poll_us | 0 | — | — | — |
| passes | 1200 | 6.000 | 6.000 | 6.000 |
| draws | 1200 | 6.000 | 6.000 | 6.000 |
| world_sprites | 1200 | 238.000 | 240.000 | 240.000 |
| world_draws | 1200 | 1.000 | 1.000 | 1.000 |
| debug_draws | 1200 | 0.000 | 0.000 | 0.000 |
| preview_draws | 1200 | 0.000 | 0.000 | 0.000 |
| gui_draws | 1200 | 0.000 | 0.000 | 0.000 |
| batch_vertex_bytes | 1200 | 142128.000 | 143568.000 | 144144.000 |
| gui_vertices | 1200 | 0.000 | 0.000 | 0.000 |
| gui_indices | 1200 | 0.000 | 0.000 | 0.000 |
| gui_vertex_bytes | 1200 | 0.000 | 0.000 | 0.000 |
| gui_index_bytes | 1200 | 0.000 | 0.000 | 0.000 |
| font_upload_bytes | 1200 | 0.000 | 0.000 | 0.000 |
| hdr_bytes | 1200 | 20160000.000 | 20160000.000 | 20160000.000 |
| preview_bytes | 1200 | 0.000 | 0.000 | 0.000 |
| font_bytes | 1200 | 0.000 | 0.000 | 0.000 |

| GPU interval | n | p50 | p95 | p99 |
|---|---:|---:|---:|---:|
| total_us | 0 | — | — | — |
| world_us | 0 | — | — | — |
| bright_us | 0 | — | — | — |
| horizontal_us | 0 | — | — | — |
| vertical_us | 0 | — | — | — |
| output_us | 0 | — | — | — |
| hud_us | 0 | — | — | — |
| debug_us | 0 | — | — | — |
| preview_us | 0 | — | — | — |
| gui_us | 0 | — | — | — |

## 1600×900 · configuration 2

Inspector on, previews off, GPU timing off. Measurement GPU states {'off': 1200}. Pass mask 319.

| CPU metric | n | p50 | p95 | p99 |
|---|---:|---:|---:|---:|
| total_us | 1200 | 125.958 | 166.709 | 202.708 |
| world_us | 1200 | 29.833 | 47.334 | 60.375 |
| bright_us | 1200 | 7.792 | 12.125 | 15.750 |
| horizontal_us | 1200 | 7.334 | 10.875 | 13.458 |
| vertical_us | 1200 | 6.708 | 10.625 | 14.625 |
| output_us | 1200 | 9.917 | 15.125 | 20.542 |
| hud_us | 1200 | 34.667 | 45.833 | 59.625 |
| debug_us | 0 | — | — | — |
| preview_us | 0 | — | — | — |
| gui_us | 1200 | 6.667 | 11.959 | 14.500 |
| update_us | 1200 | 0.042 | 0.167 | 0.334 |
| prepare_us | 1200 | 7.583 | 11.750 | 14.084 |
| gui_build_us | 1200 | 7.334 | 19.333 | 32.833 |
| query_poll_us | 0 | — | — | — |
| passes | 1200 | 7.000 | 7.000 | 7.000 |
| draws | 1200 | 8.000 | 8.000 | 8.000 |
| world_sprites | 1200 | 238.000 | 240.000 | 240.000 |
| world_draws | 1200 | 1.000 | 1.000 | 1.000 |
| debug_draws | 1200 | 0.000 | 0.000 | 0.000 |
| preview_draws | 1200 | 0.000 | 0.000 | 0.000 |
| gui_draws | 1200 | 2.000 | 2.000 | 2.000 |
| batch_vertex_bytes | 1200 | 142128.000 | 143568.000 | 144144.000 |
| gui_vertices | 1200 | 1034.000 | 1038.000 | 1038.000 |
| gui_indices | 1200 | 2130.000 | 2136.000 | 2136.000 |
| gui_vertex_bytes | 1200 | 20680.000 | 20760.000 | 20760.000 |
| gui_index_bytes | 1200 | 4260.000 | 4272.000 | 4272.000 |
| font_upload_bytes | 1200 | 0.000 | 0.000 | 0.000 |
| hdr_bytes | 1200 | 20160000.000 | 20160000.000 | 20160000.000 |
| preview_bytes | 1200 | 0.000 | 0.000 | 0.000 |
| font_bytes | 1200 | 262144.000 | 262144.000 | 262144.000 |

| GPU interval | n | p50 | p95 | p99 |
|---|---:|---:|---:|---:|
| total_us | 0 | — | — | — |
| world_us | 0 | — | — | — |
| bright_us | 0 | — | — | — |
| horizontal_us | 0 | — | — | — |
| vertical_us | 0 | — | — | — |
| output_us | 0 | — | — | — |
| hud_us | 0 | — | — | — |
| debug_us | 0 | — | — | — |
| preview_us | 0 | — | — | — |
| gui_us | 0 | — | — | — |

## 1600×900 · configuration 3

Inspector on, previews all, GPU timing off. Measurement GPU states {'off': 1200}. Pass mask 447.

| CPU metric | n | p50 | p95 | p99 |
|---|---:|---:|---:|---:|
| total_us | 1200 | 158.292 | 192.583 | 218.125 |
| world_us | 1200 | 29.459 | 40.833 | 51.250 |
| bright_us | 1200 | 7.208 | 12.167 | 14.250 |
| horizontal_us | 1200 | 6.541 | 11.084 | 14.792 |
| vertical_us | 1200 | 6.083 | 10.875 | 13.708 |
| output_us | 1200 | 9.000 | 13.375 | 17.250 |
| hud_us | 1200 | 36.292 | 43.834 | 49.167 |
| debug_us | 0 | — | — | — |
| preview_us | 1200 | 26.875 | 34.417 | 43.708 |
| gui_us | 1200 | 12.667 | 18.041 | 20.250 |
| update_us | 1200 | 0.042 | 0.125 | 0.417 |
| prepare_us | 1200 | 7.958 | 13.583 | 18.125 |
| gui_build_us | 1200 | 9.709 | 19.250 | 30.750 |
| query_poll_us | 0 | — | — | — |
| passes | 1200 | 11.000 | 11.000 | 11.000 |
| draws | 1200 | 22.000 | 22.000 | 22.000 |
| world_sprites | 1200 | 238.000 | 240.000 | 240.000 |
| world_draws | 1200 | 1.000 | 1.000 | 1.000 |
| debug_draws | 1200 | 0.000 | 0.000 | 0.000 |
| preview_draws | 1200 | 4.000 | 4.000 | 4.000 |
| gui_draws | 1200 | 12.000 | 12.000 | 12.000 |
| batch_vertex_bytes | 1200 | 144144.000 | 145584.000 | 146160.000 |
| gui_vertices | 1200 | 1754.000 | 1766.000 | 1766.000 |
| gui_indices | 1200 | 3291.000 | 3309.000 | 3309.000 |
| gui_vertex_bytes | 1200 | 35080.000 | 35320.000 | 35320.000 |
| gui_index_bytes | 1200 | 6582.000 | 6618.000 | 6618.000 |
| font_upload_bytes | 1200 | 0.000 | 0.000 | 0.000 |
| hdr_bytes | 1200 | 20160000.000 | 20160000.000 | 20160000.000 |
| preview_bytes | 1200 | 589824.000 | 589824.000 | 589824.000 |
| font_bytes | 1200 | 262144.000 | 262144.000 | 262144.000 |

| GPU interval | n | p50 | p95 | p99 |
|---|---:|---:|---:|---:|
| total_us | 0 | — | — | — |
| world_us | 0 | — | — | — |
| bright_us | 0 | — | — | — |
| horizontal_us | 0 | — | — | — |
| vertical_us | 0 | — | — | — |
| output_us | 0 | — | — | — |
| hud_us | 0 | — | — | — |
| debug_us | 0 | — | — | — |
| preview_us | 0 | — | — | — |
| gui_us | 0 | — | — | — |

## 1600×900 · configuration 4

Inspector on, previews all, GPU timing on. Measurement GPU states {'unavailable': 1200}. Pass mask 447.

| CPU metric | n | p50 | p95 | p99 |
|---|---:|---:|---:|---:|
| total_us | 1200 | 164.291 | 203.917 | 221.250 |
| world_us | 1200 | 30.625 | 42.750 | 53.958 |
| bright_us | 1200 | 7.542 | 12.375 | 16.458 |
| horizontal_us | 1200 | 6.958 | 12.416 | 15.917 |
| vertical_us | 1200 | 6.375 | 10.958 | 14.041 |
| output_us | 1200 | 9.458 | 14.125 | 16.666 |
| hud_us | 1200 | 35.208 | 45.750 | 58.000 |
| debug_us | 0 | — | — | — |
| preview_us | 1200 | 26.459 | 36.708 | 48.583 |
| gui_us | 1200 | 12.875 | 18.083 | 21.291 |
| update_us | 1200 | 0.042 | 0.209 | 0.833 |
| prepare_us | 1200 | 7.958 | 13.084 | 15.000 |
| gui_build_us | 1200 | 10.875 | 27.916 | 34.876 |
| query_poll_us | 0 | — | — | — |
| passes | 1200 | 11.000 | 11.000 | 11.000 |
| draws | 1200 | 22.000 | 22.000 | 22.000 |
| world_sprites | 1200 | 238.000 | 240.000 | 240.000 |
| world_draws | 1200 | 1.000 | 1.000 | 1.000 |
| debug_draws | 1200 | 0.000 | 0.000 | 0.000 |
| preview_draws | 1200 | 4.000 | 4.000 | 4.000 |
| gui_draws | 1200 | 12.000 | 12.000 | 12.000 |
| batch_vertex_bytes | 1200 | 144144.000 | 145584.000 | 146160.000 |
| gui_vertices | 1200 | 1844.000 | 1856.000 | 1856.000 |
| gui_indices | 1200 | 3435.000 | 3453.000 | 3453.000 |
| gui_vertex_bytes | 1200 | 36880.000 | 37120.000 | 37120.000 |
| gui_index_bytes | 1200 | 6870.000 | 6906.000 | 6906.000 |
| font_upload_bytes | 1200 | 0.000 | 0.000 | 0.000 |
| hdr_bytes | 1200 | 20160000.000 | 20160000.000 | 20160000.000 |
| preview_bytes | 1200 | 589824.000 | 589824.000 | 589824.000 |
| font_bytes | 1200 | 262144.000 | 262144.000 | 262144.000 |

| GPU interval | n | p50 | p95 | p99 |
|---|---:|---:|---:|---:|
| total_us | 0 | — | — | — |
| world_us | 0 | — | — | — |
| bright_us | 0 | — | — | — |
| horizontal_us | 0 | — | — | — |
| vertical_us | 0 | — | — | — |
| output_us | 0 | — | — | — |
| hud_us | 0 | — | — | — |
| debug_us | 0 | — | — | — |
| preview_us | 0 | — | — | — |
| gui_us | 0 | — | — | — |

## 2240×1260 · configuration 1

Inspector off, previews off, GPU timing off. Measurement GPU states {'off': 1200}. Pass mask 63.

| CPU metric | n | p50 | p95 | p99 |
|---|---:|---:|---:|---:|
| total_us | 1200 | 113.791 | 152.833 | 188.500 |
| world_us | 1200 | 33.125 | 51.750 | 88.875 |
| bright_us | 1200 | 8.375 | 13.209 | 16.209 |
| horizontal_us | 1200 | 7.917 | 11.875 | 14.791 |
| vertical_us | 1200 | 7.250 | 10.333 | 12.166 |
| output_us | 1200 | 10.500 | 16.375 | 20.417 |
| hud_us | 1200 | 34.500 | 42.333 | 47.542 |
| debug_us | 0 | — | — | — |
| preview_us | 0 | — | — | — |
| gui_us | 0 | — | — | — |
| update_us | 1200 | 0.083 | 0.292 | 0.625 |
| prepare_us | 1200 | 8.083 | 12.084 | 14.250 |
| gui_build_us | 0 | — | — | — |
| query_poll_us | 0 | — | — | — |
| passes | 1200 | 6.000 | 6.000 | 6.000 |
| draws | 1200 | 6.000 | 6.000 | 6.000 |
| world_sprites | 1200 | 238.000 | 240.000 | 240.000 |
| world_draws | 1200 | 1.000 | 1.000 | 1.000 |
| debug_draws | 1200 | 0.000 | 0.000 | 0.000 |
| preview_draws | 1200 | 0.000 | 0.000 | 0.000 |
| gui_draws | 1200 | 0.000 | 0.000 | 0.000 |
| batch_vertex_bytes | 1200 | 141552.000 | 142992.000 | 143568.000 |
| gui_vertices | 1200 | 0.000 | 0.000 | 0.000 |
| gui_indices | 1200 | 0.000 | 0.000 | 0.000 |
| gui_vertex_bytes | 1200 | 0.000 | 0.000 | 0.000 |
| gui_index_bytes | 1200 | 0.000 | 0.000 | 0.000 |
| font_upload_bytes | 1200 | 0.000 | 0.000 | 0.000 |
| hdr_bytes | 1200 | 39513600.000 | 39513600.000 | 39513600.000 |
| preview_bytes | 1200 | 0.000 | 0.000 | 0.000 |
| font_bytes | 1200 | 0.000 | 0.000 | 0.000 |

| GPU interval | n | p50 | p95 | p99 |
|---|---:|---:|---:|---:|
| total_us | 0 | — | — | — |
| world_us | 0 | — | — | — |
| bright_us | 0 | — | — | — |
| horizontal_us | 0 | — | — | — |
| vertical_us | 0 | — | — | — |
| output_us | 0 | — | — | — |
| hud_us | 0 | — | — | — |
| debug_us | 0 | — | — | — |
| preview_us | 0 | — | — | — |
| gui_us | 0 | — | — | — |

## 2240×1260 · configuration 2

Inspector on, previews off, GPU timing off. Measurement GPU states {'off': 1200}. Pass mask 319.

| CPU metric | n | p50 | p95 | p99 |
|---|---:|---:|---:|---:|
| total_us | 1200 | 126.875 | 178.500 | 201.334 |
| world_us | 1200 | 32.500 | 47.083 | 73.375 |
| bright_us | 1200 | 8.209 | 12.167 | 16.541 |
| horizontal_us | 1200 | 7.708 | 11.208 | 14.417 |
| vertical_us | 1200 | 7.250 | 10.291 | 13.000 |
| output_us | 1200 | 10.250 | 15.416 | 18.334 |
| hud_us | 1200 | 34.333 | 43.458 | 48.792 |
| debug_us | 0 | — | — | — |
| preview_us | 0 | — | — | — |
| gui_us | 1200 | 6.708 | 10.875 | 16.959 |
| update_us | 1200 | 0.042 | 0.208 | 0.667 |
| prepare_us | 1200 | 7.916 | 11.542 | 15.708 |
| gui_build_us | 1200 | 8.125 | 20.624 | 34.208 |
| query_poll_us | 0 | — | — | — |
| passes | 1200 | 7.000 | 7.000 | 7.000 |
| draws | 1200 | 8.000 | 8.000 | 8.000 |
| world_sprites | 1200 | 238.000 | 240.000 | 240.000 |
| world_draws | 1200 | 1.000 | 1.000 | 1.000 |
| debug_draws | 1200 | 0.000 | 0.000 | 0.000 |
| preview_draws | 1200 | 0.000 | 0.000 | 0.000 |
| gui_draws | 1200 | 2.000 | 2.000 | 2.000 |
| batch_vertex_bytes | 1200 | 141552.000 | 142992.000 | 143568.000 |
| gui_vertices | 1200 | 1034.000 | 1038.000 | 1038.000 |
| gui_indices | 1200 | 2130.000 | 2136.000 | 2136.000 |
| gui_vertex_bytes | 1200 | 20680.000 | 20760.000 | 20760.000 |
| gui_index_bytes | 1200 | 4260.000 | 4272.000 | 4272.000 |
| font_upload_bytes | 1200 | 0.000 | 0.000 | 0.000 |
| hdr_bytes | 1200 | 39513600.000 | 39513600.000 | 39513600.000 |
| preview_bytes | 1200 | 0.000 | 0.000 | 0.000 |
| font_bytes | 1200 | 262144.000 | 262144.000 | 262144.000 |

| GPU interval | n | p50 | p95 | p99 |
|---|---:|---:|---:|---:|
| total_us | 0 | — | — | — |
| world_us | 0 | — | — | — |
| bright_us | 0 | — | — | — |
| horizontal_us | 0 | — | — | — |
| vertical_us | 0 | — | — | — |
| output_us | 0 | — | — | — |
| hud_us | 0 | — | — | — |
| debug_us | 0 | — | — | — |
| preview_us | 0 | — | — | — |
| gui_us | 0 | — | — | — |

## 2240×1260 · configuration 3

Inspector on, previews all, GPU timing off. Measurement GPU states {'off': 1200}. Pass mask 447.

| CPU metric | n | p50 | p95 | p99 |
|---|---:|---:|---:|---:|
| total_us | 1200 | 170.708 | 239.708 | 273.708 |
| world_us | 1200 | 33.667 | 51.208 | 73.834 |
| bright_us | 1200 | 8.333 | 12.958 | 15.917 |
| horizontal_us | 1200 | 7.708 | 12.125 | 14.833 |
| vertical_us | 1200 | 7.084 | 10.500 | 13.000 |
| output_us | 1200 | 9.834 | 14.083 | 16.708 |
| hud_us | 1200 | 35.583 | 45.500 | 49.958 |
| debug_us | 0 | — | — | — |
| preview_us | 1200 | 28.875 | 36.500 | 42.542 |
| gui_us | 1200 | 15.458 | 23.750 | 29.291 |
| update_us | 1200 | 0.042 | 0.291 | 0.958 |
| prepare_us | 1200 | 8.166 | 13.125 | 17.375 |
| gui_build_us | 1200 | 11.000 | 32.917 | 48.750 |
| query_poll_us | 0 | — | — | — |
| passes | 1200 | 11.000 | 11.000 | 11.000 |
| draws | 1200 | 22.000 | 22.000 | 22.000 |
| world_sprites | 1200 | 238.000 | 240.000 | 240.000 |
| world_draws | 1200 | 1.000 | 1.000 | 1.000 |
| debug_draws | 1200 | 0.000 | 0.000 | 0.000 |
| preview_draws | 1200 | 4.000 | 4.000 | 4.000 |
| gui_draws | 1200 | 12.000 | 12.000 | 12.000 |
| batch_vertex_bytes | 1200 | 143568.000 | 145008.000 | 145584.000 |
| gui_vertices | 1200 | 1770.000 | 1782.000 | 1782.000 |
| gui_indices | 1200 | 3315.000 | 3333.000 | 3333.000 |
| gui_vertex_bytes | 1200 | 35400.000 | 35640.000 | 35640.000 |
| gui_index_bytes | 1200 | 6630.000 | 6666.000 | 6666.000 |
| font_upload_bytes | 1200 | 0.000 | 0.000 | 0.000 |
| hdr_bytes | 1200 | 39513600.000 | 39513600.000 | 39513600.000 |
| preview_bytes | 1200 | 589824.000 | 589824.000 | 589824.000 |
| font_bytes | 1200 | 262144.000 | 262144.000 | 262144.000 |

| GPU interval | n | p50 | p95 | p99 |
|---|---:|---:|---:|---:|
| total_us | 0 | — | — | — |
| world_us | 0 | — | — | — |
| bright_us | 0 | — | — | — |
| horizontal_us | 0 | — | — | — |
| vertical_us | 0 | — | — | — |
| output_us | 0 | — | — | — |
| hud_us | 0 | — | — | — |
| debug_us | 0 | — | — | — |
| preview_us | 0 | — | — | — |
| gui_us | 0 | — | — | — |

## 2240×1260 · configuration 4

Inspector on, previews all, GPU timing on. Measurement GPU states {'unavailable': 1200}. Pass mask 447.

| CPU metric | n | p50 | p95 | p99 |
|---|---:|---:|---:|---:|
| total_us | 1200 | 175.542 | 256.625 | 302.583 |
| world_us | 1200 | 34.208 | 57.334 | 74.541 |
| bright_us | 1200 | 8.458 | 12.917 | 15.959 |
| horizontal_us | 1200 | 7.750 | 12.292 | 15.000 |
| vertical_us | 1200 | 7.166 | 10.625 | 13.791 |
| output_us | 1200 | 10.042 | 15.125 | 18.666 |
| hud_us | 1200 | 35.500 | 46.167 | 50.958 |
| debug_us | 0 | — | — | — |
| preview_us | 1200 | 29.041 | 36.916 | 45.625 |
| gui_us | 1200 | 16.458 | 27.875 | 34.416 |
| update_us | 1200 | 0.042 | 0.375 | 1.125 |
| prepare_us | 1200 | 8.167 | 13.417 | 18.958 |
| gui_build_us | 1200 | 12.543 | 41.834 | 60.500 |
| query_poll_us | 0 | — | — | — |
| passes | 1200 | 11.000 | 11.000 | 11.000 |
| draws | 1200 | 22.000 | 22.000 | 22.000 |
| world_sprites | 1200 | 238.000 | 240.000 | 240.000 |
| world_draws | 1200 | 1.000 | 1.000 | 1.000 |
| debug_draws | 1200 | 0.000 | 0.000 | 0.000 |
| preview_draws | 1200 | 4.000 | 4.000 | 4.000 |
| gui_draws | 1200 | 12.000 | 12.000 | 12.000 |
| batch_vertex_bytes | 1200 | 143568.000 | 145008.000 | 145584.000 |
| gui_vertices | 1200 | 1860.000 | 1872.000 | 1872.000 |
| gui_indices | 1200 | 3459.000 | 3477.000 | 3477.000 |
| gui_vertex_bytes | 1200 | 37200.000 | 37440.000 | 37440.000 |
| gui_index_bytes | 1200 | 6918.000 | 6954.000 | 6954.000 |
| font_upload_bytes | 1200 | 0.000 | 0.000 | 0.000 |
| hdr_bytes | 1200 | 39513600.000 | 39513600.000 | 39513600.000 |
| preview_bytes | 1200 | 589824.000 | 589824.000 | 589824.000 |
| font_bytes | 1200 | 262144.000 | 262144.000 | 262144.000 |

| GPU interval | n | p50 | p95 | p99 |
|---|---:|---:|---:|---:|
| total_us | 0 | — | — | — |
| world_us | 0 | — | — | — |
| bright_us | 0 | — | — | — |
| horizontal_us | 0 | — | — | — |
| vertical_us | 0 | — | — | — |
| output_us | 0 | — | — | — |
| hud_us | 0 | — | — | — |
| debug_us | 0 | — | — | — |
| preview_us | 0 | — | — | — |
| gui_us | 0 | — | — | — |
