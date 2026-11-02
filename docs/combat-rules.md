# Combat rules (M6.1)

CombatConfig defaults: player HP 5, speed 180, invulnerability .6s; enemy HP 2, speed 65;
projectile damage 1, speed 420, lifetime 2s; firing interval .6s; initial enemies 8;
active limits 48 enemies / 64 projectiles; spawn interval 1.5s decreasing linearly to .6s
over 90 seconds. This ramp duration is configurable, and is not a victory timer.

HP and damage must be positive. Speeds must be finite in [0,100000]. Durations must be
finite from one fixed tick through one day. Capacities must be 1..256, initial enemies
cannot exceed their cap, and minimum spawn interval cannot exceed initial interval.
Input NaN/Inf throws before advancing the tick. Any finite nonzero input is normalized;
zero input stops movement.

Player and enemy sprites are 24x24 with radius-12 colliders offset to their center.
Projectiles are 8x8 with radius-4 colliders. Position in Scene is the sprite top-left;
aim and chase use world collider centers. Player is blue, enemy red/orange, projectile
yellow. A damaged player turns pale until vulnerable; death retains a gray-blue sprite.

The first firing opportunity is tick 36 with defaults. Nearest living enemy is chosen
by squared center distance. Equal distances use full ObjectId map order. A coincident
target uses +X direction. No target or full capacity consumes the opportunity; no backlog
is retained. Player death stops future movement and firing. It does not erase the player.

An enemy contact on tick T subtracts one HP, never below zero, and sets the next vulnerable
tick to T+36. T+1 through T+35 cannot hurt; T+36 can hurt. Both Enter and Stay qualify.
Multiple simultaneous contacts can subtract only one HP. Projectile damage is resolved
first, so an enemy killed during this tick cannot inflict contact damage in that tick.

Collision events already have stable sorted pair ordering in Scene. A projectile consumes
itself on the first living enemy candidate in that order. Further candidates for that
projectile are ignored. If an earlier projectile killed an enemy, a later projectile
skips it and may hit another living candidate. Death scores exactly once during cleanup.
All other role pairs (including enemy/enemy and projectile/player) do no combat damage.
Scene contacts() is geometric overlap count, not a damage counter.

A projectile created at tick T with lifetime L ticks participates at T..T+L-1 and is
removed before collision at T+L. Consumed projectiles are deleted after event resolution.
There are no persistent raw object pointers. Runtime counts reflect actual live entries;
spawned includes initial enemies, fired counts successful automatic projectile creation,
hits counts resolved projectile impacts, kills counts enemy deaths.

Initial and later spawning uses a fixed eight-direction table and a creation counter,
at radii 320,360,400,440 around the player's current center. No clock seed or standard
random distribution is used. Each spawn deadline schedules one new opportunity using
the current ramp interval. At capacity the opportunity is skipped, without backlog.

## Limits

Collision remains discrete O(N²), reusing Scene/Collision2D. Default projectile movement
is 7 units/tick versus a combined enemy/projectile radius of 16: ordinary direct hits
are practical, but grazing, fast relative movement and configured higher speeds can
tunnel. No continuous collision detection, collision layer, spatial hash, avoidance,
world bounds or separation response is added. Enemies can overlap each other and player.
No win state, Game Over UI, restart, HUD, or M6.2 gameplay is implemented.
