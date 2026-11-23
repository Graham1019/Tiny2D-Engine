# M7.1 measurement protocol

Release, single sequential run per cell in this order: logical 800×450 on/off, then 1120×630 on/off. Seed 71, Final, HUD shown, no live inputs, 1320 successful frames per run. Exclude the first 120, sample the next 1200. Record actual framebuffer pixels. Keep observed tails. No screenshot readback during measurement.

The monotonic CPU frame clock starts after event polling, before size/availability checks, and stops after state restoration. It includes one fixed update, CPU geometry and HUD preparation and driver submission. Swap, explicit waits, reporting, sampling storage and readback are outside. Finite runs request swap interval zero. Driver back-pressure may appear inside submission. Per-pass clocks include binding, draws and error checks. A skipped pass contributes zero. Nearest-rank percentiles in microseconds, counters in their named units. Compare on minus off and on/off. This stage establishes costs without an FPS target.
