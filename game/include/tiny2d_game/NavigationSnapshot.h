#pragma once

#include "tiny2d/GridNavigator.h"
#include "tiny2d/MapSnapshot.h"
#include "tiny2d_game/UpgradeSystem.h"

namespace tiny2d_game {

inline constexpr std::uint32_t simulationRulesVersion = 1;

enum class NavigationPhase { Playing, AwaitingUpgrade, Victory, GameOver };

struct NavigationEnemy : tiny2d::WaypointAgent {
    std::size_t slot = 0; // (epoch, slot) identifies an active enemy.
};

struct UpgradeSnapshot {
    std::array<int, 4> levels{};
    std::uint64_t rng = 0, serial = 0;
    std::optional<UpgradeOffer> offer;
    std::optional<UpgradeChoice> lastChoice;
};

struct NavigationSnapshot {
    tiny2d::MapSnapshot world;
    NavigationPhase phase = NavigationPhase::Playing;
    int hp = 0;
    std::uint64_t vulnerableAt = 0, nextOfferTick = 0;
    int requestedEnemies = 0, spawnMinimumDistance = 0;
    std::optional<std::size_t> lastDamageSlot;
    std::uint64_t spawnRng = 0;
    std::vector<NavigationEnemy> enemies;
    UpgradeSnapshot upgrades;
};

// In-memory restore measurements. Neither field belongs to the snapshot.
struct RestoreTimings {
    double semanticValidationUs = 0;
    double cacheRebuildUs = 0;
};

} // namespace tiny2d_game
