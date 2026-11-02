#pragma once
#include "CombatConfig.h"
#include "GameplayInput.h"
#include "tiny2d/Scene.h"
#include <map>
namespace tiny2d_game {
enum class EntityKind { Player, Enemy, PlayerProjectile };
struct EntityState {
    EntityKind kind;
    uint64_t serial=0;
    int health=0;
    uint64_t expires=0;
};
// CPU-only, single-threaded. Public snapshots are borrowed until the next update.
class GameSession {
public:
    explicit GameSession(const CombatConfig& config={});
    void fixedUpdate(const GameplayInput& input={});
    const tiny2d::Scene& scene() const { return scene_; }
    const std::map<tiny2d::ObjectId,EntityState>& entities() const { return entities_; }
    tiny2d::ObjectId playerId() const { return player_; }
    int playerHealth() const { return entities_.at(player_).health; }
    int playerMaxHealth() const { return config_.playerMaxHealth; }
    bool playerAlive() const { return playerHealth()>0; }
    uint64_t tickCount() const { return tick_; }
    double elapsedSimulationTime() const;
    uint64_t killCount() const { return kills_; }
    uint64_t enemyCount() const;
    uint64_t projectileCount() const;
    uint64_t spawnedCount() const { return spawned_; }
    uint64_t firedCount() const { return fired_; }
    uint64_t hitCount() const { return hits_; }
private:
    friend struct GameSessionTestAccess;
    tiny2d::ObjectId create(EntityKind, tiny2d::Vector2 center);
    void spawnEnemy();
    tiny2d::Vector2 center(tiny2d::ObjectId) const;
    void fire();
    void erase(tiny2d::ObjectId);
    CombatConfig config_;
    tiny2d::Scene scene_;
    std::map<tiny2d::ObjectId,EntityState> entities_;
    tiny2d::ObjectId player_;
    uint64_t tick_=0, serial_=0, kills_=0, spawned_=0, fired_=0, hits_=0;
    uint64_t nextFire_=0, nextSpawn_=0, vulnerableAt_=0;
};
}
