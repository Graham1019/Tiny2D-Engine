#pragma once

#include "tiny2d_game/NavigationSnapshot.h"
#include "tiny2d/MapSession.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace tiny2d_game {

const char* phaseName(NavigationPhase phase);

struct NavigationTimings {
    double enemyUpdateUs = 0;    // Entire stable-slot motion loop.
    double enemyCollisionUs = 0; // Tile sweeps inside motion, plus contact resolution.
    double upgradeUpdateUs = 0;  // Offer scheduling, or the most recent choice.
};

class NavigationSession {
public:
    static constexpr std::uint64_t durationTicks = 3600;
    static constexpr std::uint64_t offerInterval = 600;
    static constexpr int maxEnemies = 256;
    static tiny2d::Vector2 enemyHalfSize() { return {5.f, 5.f}; }

    explicit NavigationSession(std::uint32_t seed = 71,
                               const tiny2d::MapParameters& parameters = {}, int enemyCount = 16);
    NavigationSnapshot captureSnapshot() const;
    static NavigationSession fromSnapshot(const NavigationSnapshot& snapshot, RestoreTimings* timings = nullptr);
    void restoreSnapshot(const NavigationSnapshot& snapshot);

    const tiny2d::MapSession& world() const { return world_; }
    const tiny2d::GridNavigator& navigator() const { return navigator_; }
    const std::vector<NavigationEnemy>& enemies() const { return enemies_; }
    const UpgradeSystem& upgrades() const { return upgrades_; }
    const NavigationTimings& timings() const { return timings_; }
    NavigationPhase phase() const { return phase_; }
    int hp() const { return hp_; }
    std::uint64_t vulnerableAt() const { return vulnerableAt_; }
    std::uint64_t remainingTicks() const { return durationTicks - world_.tick(); }
    std::uint64_t nextOfferTick() const { return nextOfferTick_; }
    std::uint64_t spawnRngState() const { return spawnRng_; }
    int requestedEnemies() const { return requestedEnemies_; }
    int spawnMinimumDistance() const { return spawnMinimumDistance_; }
    const char* spawnShortfallReason() const;
    std::optional<std::size_t> lastDamageSlot() const { return lastDamageSlot_; }

    void setPaused(bool paused) { world_.setPaused(paused); }
    void fixedUpdate(tiny2d::MapInput input = {});
    void step(tiny2d::MapInput input = {});
    bool choose(std::uint64_t epoch, std::uint64_t serial, UpgradeId id);
    void reset();
    void regenerate(std::uint32_t seed);
    void regenerate(std::uint32_t seed, const tiny2d::MapParameters& parameters);

private:
    friend struct NavigationSessionTestAccess;
    NavigationSession(tiny2d::MapSession&& world, int enemyCount);
    NavigationSession(tiny2d::MapSession&& world, const NavigationSnapshot& snapshot);
    void spawnEnemies();
    void resolveContact();

    tiny2d::MapSession world_;
    tiny2d::GridNavigator navigator_;
    std::vector<NavigationEnemy> enemies_;
    UpgradeSystem upgrades_;
    std::uint64_t spawnRng_;
    int requestedEnemies_, spawnMinimumDistance_ = 0;
    int hp_ = 5;
    std::uint64_t vulnerableAt_ = 0, nextOfferTick_ = offerInterval;
    NavigationPhase phase_ = NavigationPhase::Playing;
    std::optional<std::size_t> lastDamageSlot_;
    NavigationTimings timings_;
};

} // namespace tiny2d_game
