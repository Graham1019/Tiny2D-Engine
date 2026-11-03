# Bounded VFX lifecycle

128 effect slots, overwritten in deterministic circular insertion order. Each slot
has at most four Sprite slots; trails use two. Another 49 slots cover player/enemy
center marks for the production 48-enemy cap. Total world slots: 561. UI owns four
edge slots. Empty/expired slots remain allocated but disabled: this is fixed capacity
reuse, not a retained history leak. reset disables every slot and clears all effect
records, sequence cursor, damage timer and counters. Next consume detects the new
Scene token. No per-hit Scene/GameObject creation or destruction occurs after setup.

During present, existing objects receive positions, tints and optional Sprite values.
SceneRenderer can still allocate its normal temporary command lists; this change does
not claim a globally allocation-free render path. The event ring itself never allocates.
GPU objects are never constructed by CombatVFX; shared white texture, shader and buffers
remain owned by the existing renderer and are destroyed before Application's Window.

At most 128 visual instances survive bursts; excess insertions overwrite circular
slots. Effects and overwritten event history both contribute to dropped(). Rendering
uses fixed slot order within each render order, ensuring deterministic transparency;
it does not promise chronological alpha order after ring wrap. Hits may be replaced
under extreme visual load, with no effect on gameplay or scoring.

Validation includes 30,000 paired gameplay opportunities with/without presentation,
30,000 high-density generation/expiry updates, 120 terminal/restart loops, ring wrap,
expiry, Scene identity reset and collider absence. GPU tests render 10,001 frames,
create repeated hits/kills through the first 5400-tick session, then keep rendering
Victory and expired effects. Pixel checks cover hit/death colors, fades, trail residue,
UI damage, camera independence, resize and reset; texture handle remains unchanged.
Historical M51/M61/M62 tests retain flush ownership, cancellation and context coverage.

Fixed container bounds and stable texture handles are evidence for these paths, not
whole-process/driver memory-leak certification. No GPU memory analyzer was available.
