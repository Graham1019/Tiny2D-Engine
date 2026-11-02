# Difficulty design

Validated API: difficultyLevel(unsigned), makeDifficultyConfig(DifficultyLevel),
difficultyName(DifficultyLevel). Both config/name APIs reject invalid enum casts.
Only validated values enter the flow. Default GameSession{} remains M6.2 / Lv.1.
For d=1..5 and k=d-1:

- Enemy speed = 65 * (1 + .15*k)
- Initial/minimum spawn seconds = 1.5 / (1 + .20*k), .6 / (1 + .20*k)
- Fire seconds = .6 / (1 + .08*k)
- Projectile speed = 420 * (1 + .05*k)

All deadlines use the shared combatTicks function: ceil(seconds*60 - 1e-10).
Validated CombatConfig durations are its precondition. No float equality schedules events.
The table is emitted by the executable Difficulty test, which asserts each tick value:

|Level|Enemy speed|Initial spawn ticks|Min spawn ticks|Fire ticks|Projectile speed|
|---|---|---|---|---|---|
|1|65|90|36|36|420|
|2|74.75|75|30|34|441|
|3|84.5|65|26|32|462|
|4|94.25|57|23|30|483|
|5|104|50|20|28|504|

Seconds remain the formula results, not the rounded ticks divided back into seconds.
HP 5, player speed 180, enemy HP 2, damage 1, immunity .6s / 36 ticks,
projectile lifetime 2s / 120 ticks, initial enemies 8, capacities 48/64,
spawn ramp 90s and survival 5400 ticks remain unchanged at every level.
Spawn ramp evaluates the original seconds curve at each integer deadline, then
quantizes the next interval. Full capacity consumes opportunities without backlog.
Actual live enemies and kills need not be monotonic with difficulty.

See [measurements](data/m63a/performance.md) for observed active counters and timings.
