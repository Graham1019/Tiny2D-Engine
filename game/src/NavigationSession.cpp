#include "tiny2d_game/NavigationSession.h"
#include "tiny2d/FixedTimestep.h"
#include "NavigationRandom.h"
#include "NavigationSpawn.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace tiny2d_game {
namespace {
using Clock = std::chrono::steady_clock;
double elapsedUs(Clock::time_point start) {
    return std::chrono::duration<double, std::micro>(Clock::now() - start).count();
}

bool overlap(tiny2d::Vector2 a, tiny2d::Vector2 aHalf, tiny2d::Vector2 b, tiny2d::Vector2 bHalf) {
    return std::abs(double(a.x) - b.x) < double(aHalf.x) + bHalf.x &&
           std::abs(double(a.y) - b.y) < double(aHalf.y) + bHalf.y;
}
}

const char* phaseName(NavigationPhase phase) {
    switch (phase) {
    case NavigationPhase::Playing: return "Playing";
    case NavigationPhase::AwaitingUpgrade: return "AwaitingUpgrade";
    case NavigationPhase::Victory: return "Victory";
    case NavigationPhase::GameOver: return "GameOver";
    }
    throw std::invalid_argument("unknown navigation phase");
}

NavigationSession::NavigationSession(std::uint32_t seed, const tiny2d::MapParameters& parameters,
                                     int enemyCount)
    : NavigationSession(tiny2d::MapSession(seed, parameters), enemyCount) {}

NavigationSession::NavigationSession(tiny2d::MapSession&& world, int enemyCount)
    : world_(std::move(world)), upgrades_(world_.seed()),
      spawnRng_(initialSpawnRng(world_.seed())), requestedEnemies_(enemyCount) {
    if (enemyCount < 0 || enemyCount > maxEnemies)
        throw std::invalid_argument("navigation enemy count must be within 0..256");
    navigator_.refresh(world_.map(), world_.epoch(), enemyHalfSize(), world_.map().spawn());
    spawnEnemies();
}

SpawnSites navigationSpawnSites(const tiny2d::MapSession& world, const tiny2d::DistanceField& field) {
    const auto& map = world.map();
    if (field.distance(map.spawn()) != 0)
        throw std::invalid_argument("spawn is not a valid navigation target");
    int farthest = 0;
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x)
            farthest = std::max(farthest, field.distance({x,y}));
    SpawnSites sites{std::clamp(farthest / 2, 1, 36), {}};
    const auto spawn = map.spawn();
    const auto player = map.tileToWorldCenter(spawn.x, spawn.y);
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            if (field.distance({x,y}) >= sites.minimumDistance &&
                !overlap(map.tileToWorldCenter(x,y), NavigationSession::enemyHalfSize(),
                         player, world.playerHalfSize()))
                sites.cells.push_back({x,y});
        }
    }
    return sites;
}

void NavigationSession::spawnEnemies() {
    const auto& map = world_.map();
    auto sites = navigationSpawnSites(world_, navigator_.field());
    spawnMinimumDistance_ = sites.minimumDistance;
    auto& candidates = sites.cells;
    const auto count = std::min(candidates.size(), static_cast<std::size_t>(requestedEnemies_));
    enemies_.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto pick = i + drawNavigationIndex(spawnRng_, static_cast<unsigned>(candidates.size() - i));
        std::swap(candidates[i], candidates[pick]);
        const auto cell = candidates[i];
        NavigationEnemy enemy;
        enemy.position = map.tileToWorldCenter(cell.x, cell.y);
        enemy.epoch = world_.epoch();
        enemy.slot = i;
        enemies_.push_back(enemy);
    }
}

const char* NavigationSession::spawnShortfallReason() const {
    return enemies_.size() < static_cast<std::size_t>(requestedEnemies_)
        ? "insufficient distinct reachable spawn tiles at minimum path distance" : "";
}

void NavigationSession::resolveContact() {
    if (world_.tick() < vulnerableAt_) return;
    for (const auto& enemy : enemies_) {
        if (overlap(enemy.position, enemyHalfSize(), world_.playerPosition(), world_.playerHalfSize())) {
            --hp_;
            vulnerableAt_ = world_.tick() + upgrades_.guardTicks();
            lastDamageSlot_ = enemy.slot;
            return;
        }
    }
}

void NavigationSession::fixedUpdate(tiny2d::MapInput input) {
    if (!world_.paused()) step(input);
}

void NavigationSession::step(tiny2d::MapInput input) {
    if (phase_ != NavigationPhase::Playing) return;
    world_.step(input, upgrades_.playerSpeed());
    const auto& map = world_.map();
    navigator_.refresh(map, world_.epoch(), enemyHalfSize(), map.worldToTile(world_.playerPosition()));

    const auto motionStart = Clock::now();
    timings_ = {};
    const float stride = static_cast<float>(upgrades_.enemySpeed() * tiny2d::FixedTimestep::step);
    for (auto& enemy : enemies_)
        timings_.enemyCollisionUs += navigator_.advance(map, enemy, stride);
    timings_.enemyUpdateUs = elapsedUs(motionStart);
    const auto contactStart = Clock::now();
    resolveContact();
    timings_.enemyCollisionUs += elapsedUs(contactStart);

    const auto upgradeStart = Clock::now();
    if (hp_ == 0) phase_ = NavigationPhase::GameOver;
    else if (world_.tick() >= durationTicks) phase_ = NavigationPhase::Victory;
    else if (world_.tick() == nextOfferTick_) {
        nextOfferTick_ += offerInterval;
        if (upgrades_.createOffer(world_.epoch())) phase_ = NavigationPhase::AwaitingUpgrade;
    }
    timings_.upgradeUpdateUs = elapsedUs(upgradeStart);
}

bool NavigationSession::choose(std::uint64_t epoch, std::uint64_t serial, UpgradeId id) {
    const auto start = Clock::now();
    if (phase_ != NavigationPhase::AwaitingUpgrade || !upgrades_.choose(epoch, serial, id))
        return false;
    if (id == UpgradeId::Vitality) ++hp_;
    phase_ = NavigationPhase::Playing;
    timings_.upgradeUpdateUs = elapsedUs(start);
    return true;
}

void NavigationSession::reset() {
    regenerate(world_.seed(), world_.parameters());
}

void NavigationSession::regenerate(std::uint32_t seed) {
    regenerate(seed, world_.parameters());
}

void NavigationSession::regenerate(std::uint32_t seed, const tiny2d::MapParameters& parameters) {
    // Carry epoch and pause through the existing MapSession replacement path.
    // Navigation and spawn validation may still fail after the map succeeds.
    auto nextWorld = world_;
    nextWorld.regenerate(seed, parameters);
    NavigationSession candidate(std::move(nextWorld), requestedEnemies_);
    static_assert(std::is_nothrow_move_assignable_v<NavigationSession>);
    *this = std::move(candidate);
}

} // namespace tiny2d_game
