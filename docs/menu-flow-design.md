# Menu and session flow

DifficultySelection -> number-key edges select Lv.1..5 -> Enter edge -> ActiveGame.
Default selection is Lv.3 NORMAL. There is no GameSession in the menu, so fixedUpdate
is a no-op and game() rejects access. The inner GamePhase remains Playing/Victory/GameOver.
ActiveGame ignores number keys; difficulty() now describes the immutable applied level.
Terminal R edges call the existing SessionController with the same saved CombatConfig.
No return-to-menu shortcut is added.

Each input sample contains five number states, Enter, R and focus. Held keys do not
repeat. Simultaneous numeric edges resolve in ascending order (highest wins), then
Enter starts that selection. Focus loss disarms action input; restoring focus requires
all action keys to be released before rearming. R is synchronized on release. Escape
remains the application exit action. GameplayInput contains movement only.

Creation and restart construct replacement ownership before swap. Allocation failure
consumes the edge but preserves the prior menu/session; release and repress retries.
All mutation occurs at application update boundaries outside render passes.
Successful creation/restart resets camera offset, zoom and FixedTimestep; that update
advances no gameplay tick. New Scene tokens invalidate old ObjectIds. Renderer and
white texture survive every restart. Focus loss/zero framebuffer resets the accumulator.
Menu rendering with a zero framebuffer never accesses a nonexistent session.

DifficultyMenu builds a cached CPU Scene from BitmapFont glyph rectangles and asset 0.
Resize or selection rebuilds the scene transactionally. UI camera is centered in logical
window coordinates, zoom 1; no player/world camera input reaches menu presentation.
HUD receives the applied level from the owning flow and includes it in its cache key.
The label is at the lower left, leaving HP/time/kills and terminal panel unchanged.

CLI: without --frames, launch the menu; --difficulty N sets its initial selection.
With positive --frames, automatically start the selected level (default 3), ignoring
live movement/restart input, one fixed opportunity per render frame. Terminal updates
freeze while rendering continues. Survivor rejects --frames 0 and invalid difficulty;
--difficulty on another scene is rejected. Other scenes retain previous CLI behavior.
