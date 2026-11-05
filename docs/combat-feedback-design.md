# Combat feedback (M6.3b)

The five difficulty formulas, combat HP/damage, cooldowns, spawning and survival
rules are frozen. GameSession emits ProjectileHit after a valid damage application,
PlayerDamaged only when HP decreases, and EnemyKilled once during existing cleanup.
No extra collision query runs. Positions are copied before destruction; no VFX owns
an entity pointer or Gameplay ObjectId. Sequence order follows existing collision
resolution, then player damage, then ordered kill cleanup.

Each session owns a 512-entry std::array ring, with monotonically increasing sequence
and original simulation tick. event(sequence) returns a borrowed pointer or null for
invalid/overwritten history. There is no allocating event bus or queue drain that
changes gameplay. Consumers use their own cursor. Overflow overwrites oldest history;
CombatVFX accounts for lost events in dropped(). The adapter consumes after every
fixed step (including up to six catch-up steps), before rendering. The ring also
preserves intervening ticks for consumers reading less frequently. New session Scene
identity resets presentation cursor and effects before consuming new events.

CombatVFX runs on CPU and reads a const GameSession. Its bounded effect records and
Scene slots have no colliders, Window, shaders or texture ownership. Existing white
asset 0 is used for all marks, sparks, trails and screen edges.

|Feedback|Duration|Presentation|
|---|---|---|
|Hit|0.15 s|4 yellow sparks, fixed cardinal directions, 55 units/s|
|Death|0.30 s|4 orange/red sparks, 85 units/s, shrinking and fading|
|Trail|0.08 s|2 translucent squares behind each sampled projectile position|
|Damage|0.12 s|red UI edges and decaying world-camera offset|

Sparks fade linearly as 1-age/lifetime. Death effects outlive removed enemies. Trail
positions/velocities are copied once per observed tick; no terminal trail generation.
All effects expire with presentation delta, including terminal frames. Interactive
unfocused/minimized updates pause presentation; a later large delta safely expires
old effects. Finite CLI uses 1/60 presentation seconds per frame. None of these times
enter damage, firing, spawn or survival calculations. No hit stop is implemented.

Camera amplitude a=3*remaining/0.12 logical pixels. Offset is
(a*cos(elapsed*180), 0.5*a*sin(elapsed*220)); zero after expiry. Divide by world zoom
before adding to camera position, preserving screen amplitude. This never writes a
GameSession Transform. UI camera remains centered and unshaken.

Blue center marker distinguishes the player; dark red centers clarify enemy silhouettes.
Markers render before effect sparks (a pixel regression caught the inverse order).
Grid contrast is reduced. Victory/GameOver panels have green/red accent rules; existing
text and restart semantics are unchanged. Red screen edges submit after HUD in its
existing UI pass. World submits grid, gameplay, then VFX in one pass; no new renderer.

## M6.3b.1 review update

Death > Hit > Trail now protects active hit/death feedback from trail saturation.
CPU VFX/HUD preparation precedes both guarded World/UI passes. See
[review fixes](m63b1-review-fixes.md) for deterministic eviction/drop semantics,
exception/Context contracts, new tests and latest measurements. The M6.3b results
above remain historical; new data lives in data/m63b1 without overwriting them.
