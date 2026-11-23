# Release measurement protocol

Run one cell at a time in logical-size order 800×450 then 1120×630, configurations 1–4 at each size. Record the framebuffer dimensions from each CSV. Use the same Release binary, seed 71, Final, Bloom on at 1.2, HUD on, no selection, PickBounds off, selection outline enabled. The finite input path fixes the initial layout and performs exactly one tick per drawable frame. No capture during measurement. Frame IDs 1–120 warm up, 121–1320 are the 1200 candidates for both clocks.

1. Inspector off, previews off, timing off.
2. Inspector on, previews off, timing off.
3. Inspector on, four thumbnails, timing off.
4. Inspector on, four thumbnails, timing requested.

Run `python3 docs/data/m72/measure.py run` from the repository root. Each log begins with the actual command. The reducer checks every measurement frame's pass mask, GUI submission, preview draw count, total draws and batch payload. Recompute with `python3 docs/data/m72/measure.py summarize`.

Each CPU row describes one submitted frame, with the query state as issued. A GPU completion or invalid retirement has its own row keyed by the original frame ID. Its received_frame gives the polling boundary. For final status, join a retirement row to its issuing frame. An outstanding pending row at exit has empty GPU durations. Filtering by the issuing measurement flag excludes late warmup results. On unsupported hardware the timestamp bits and status distinguish this from a disabled timer.

Use nearest rank (sort then select ceil(p × n), one-based). An empty cell denotes an unexecuted interval. Times are µs, payload/storage columns are bytes. Draw, sprite, vertex and index counts are integral. CPU total runs from after pollEvents through renderer/state restoration, before screenshot, swap and CSV. GPU total spans renderer submission including enabled previews, debug lines and GUI. Displayed distributions use complete samples only. No extra simulation ticks, blocking timestamp reads, or forced drain at exit.
