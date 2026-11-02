# Gameplay architecture (M6.2)

GameSession owns a Scene and a map keyed by the complete ObjectId (sceneId/index/generation).
EntityState contains an explicit EntityKind (Player, Enemy, PlayerProjectile), health, projectile
expiry tick and a monotonic creation serial. GameObject remains unchanged: transform, velocity,
sprite resource key and collider only. No engine core source was changed.

The public Scene and entity map are read-only. The session is the only production owner that
creates or destroys combat objects. Borrowed references must not survive fixedUpdate. Every
access resolves its handle through Scene; cleanup destroys the Scene object and erases its
gameplay entry together. Scene invalidates the old generation immediately. Creation rolls back
the Scene object if state insertion fails. Updates, like Scene updates, are not transactional
under allocation failure. Test fixtures use a private friend probe, not a mutable production API.

The tiny2d_game static library compiles only GameSession, Scene, Collision2D and Vector2. It
has no GL/platform link dependency. The three gameplay test executables link only this library
and run without a window/context. The existing top-level CMake still discovers macOS engine
dependencies even in headless mode; headless here means context-free testing, not a portable
non-macOS configure. CPU scene sources are compiled into both standalone gameplay and the
existing engine archive; the final link selects the needed definitions.

SurvivorDemo translates GLFW keys into GameplayInput, advances GameSession and submits its
Scene through the unchanged SceneRenderer. A separate decorative grid Scene has no colliders.
Texture2D exists only in the renderer adapter. The derived Application destroys SceneRenderer
and its texture owners before the base Window. SceneRenderer keeps the shared white texture
alive through end/cancel. No ECS is needed for three roles and bounded hundreds of objects.

## Fixed update contract

Ticks start at zero. Each call validates finite input before mutation, then:

1. Increment tick; remove projectiles whose exclusive expiry has arrived.
2. Normalize nonzero input to player speed; dead players receive zero velocity.
3. If the fire deadline arrived, consume this opportunity and aim/fire if alive and under cap.
4. If the spawn deadline arrived, consume this opportunity and spawn if under cap.
5. Set all enemy velocities toward the player, or zero after player death.
6. Scene::update(1/60) integrates velocity and runs its existing discrete collision detector.
   All objects created before this call participate in this tick.
7. Collect Enter/Stay events without mutating Scene from the callback.
8. Resolve projectile hits in Scene's ordered collision-pair order, marking each projectile
   consumed once. Dead enemies are excluded from subsequent candidates.
9. Resolve at most one surviving enemy contact with the player, if vulnerable.
10. Count dead enemies once, destroy them and consumed projectiles, erase gameplay state,
    update the player damage/death tint.

New projectiles move on their creation tick. New enemies may collide on creation tick but
cannot be targeted by that tick's earlier firing opportunity. M6.2 decides GameOver/Victory after cleanup and freezes all subsequent simulation.

Times quantize upward to integer ticks (ceil(seconds * 60), with a tiny rounding tolerance
for exact represented tick multiples). Deadlines do not accumulate render delta error.
Finite demo mode ignores live movement input and provides one tick opportunity per rendered frame (terminal calls are no-ops);
interactive mode uses the existing FixedTimestep accumulator (six steps maximum per frame).
Camera controls affect presentation only.

Determinism covers repeated sessions with the same config/input in this build. Independent
Scene tokens differ; tests compare creation serials, role/health, positions and real counters.
No claim of cross-platform floating-point bit identity is made.

M6.2 adds SurvivalRules, GamePhase and CPU SessionController. See [game loop](game-loop-design.md) and [HUD](hud-rendering.md).
