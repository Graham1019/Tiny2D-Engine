# M6.2 screen HUD

GameHUD reads GameSession and generates a CPU-only Scene. BitmapFont contains
original 5x7 row masks for A-Z, 0-9, colon, slash, minus and space. Set pixels become
white-asset rectangles with tint. All glyphs share asset 0 and the existing shader.
No atlas, font dependency, new vertex format or GPU resource is introduced.

World pass submits grid and gameplay, ends, and captures its actual draw count.
UI pass then begins with a separate camera, submits HUD, ends, and captures its
actual draw count. Total is their sum, including automatic capacity flushes.
Each begin resets RenderStats; the last pass alone is not a frame statistic.
The original sandbox scenes never instantiate GameHUD.

UI camera viewport uses logical Window size, center (width/2,height/2), zoom 1.
Existing orthographic math maps logical (0,0) to top left with positive Y down.
Window maintains the framebuffer viewport separately. Retina scaling therefore
changes pixel density, not layout. GPU work is skipped for nonpositive framebuffer
or logical size. Each resize invalidates layout. World pan/zoom never affects UI.

HP and bar sit left, ceil(remainingTicks/60) formatted MM:SS in the center, kills
right. Tick 0 and 1 show 01:30; tick 60 shows 01:29. Death preserves remaining time.
Layout scale is min(2,width/320,height/240). Terminal panel contains title, actual
floor(elapsedTicks/60) survived time, kills, and restart instruction. Panel blends
over retained world. HUD Scene order is stable insertion order.

A key caches health, kills, displayed seconds, phase, feedback and viewport.
Unchanged frames do not create/destroy glyph objects. A changed key constructs a
replacement Scene before swapping. Storage does not accumulate old scenes. This
simple approach trades occasional CPU rebuilding for zero texture uploads.
Kill flash expiry creates one further rebuild; terminal HUD stays cached.
The default overlay remains under 1000 sprites; larger text may flush normally.
Extreme custom HP/duration values can exceed the compact header's intended width;
320x240 and 800x600 are tested with default rules and representative kill counts.

See data/m62/performance.csv for separate fixed update, HUD update and world/UI
CPU submission samples. Submission timings are not GPU execution measurements.
M51GPU and M61GPU continue to test preflight, cancel, texture owners and context
rejection; M62GPU adds UI pixel, overlay, restart and resize checks.

## M6.3a difficulty and menu

Survivor now opens a CPU-only difficulty-selection flow (default Lv.3); 1–5 selects,
Enter starts, and terminal R restarts the applied level. Direct GameSession defaults
remain Lv.1/M6.2. HUD shows the applied level at lower left using the existing UI pass.
Finite --frames runs auto-start; --difficulty accepts 1..5.
See [M6.3a validation and commands](milestone-06-3a.md), [difficulty design](difficulty-design.md)
and [menu lifecycle](menu-flow-design.md).
