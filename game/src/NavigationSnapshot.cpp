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
using tiny2d::TileCoord;
using tiny2d::Vector2;
using Clock = std::chrono::steady_clock;
using Microseconds = std::chrono::duration<double, std::micro>;

void require(bool ok, const char* field) {
    if (!ok) throw std::invalid_argument(field);
}

bool same(Vector2 a, Vector2 b) {
    return a.x == b.x && a.y == b.y;
}

bool overlapsPlayer(Vector2 enemy, Vector2 player) {
    return std::abs(double(enemy.x) - player.x) < 10. &&
           std::abs(double(enemy.y) - player.y) < 10.;
}

// An unfinished leg is axial and belongs to one actual graph edge, including
// the half after crossing into the destination tile. The field may have
// changed target after the commitment, so its current gradient is irrelevant.
bool onLeg(const tiny2d::TileMap& map, const tiny2d::NavigationGrid& grid,
           Vector2 position, TileCoord destination, bool recentering) {
    if (!grid.enterable(destination)) return false;
    const Vector2 end = map.tileToWorldCenter(destination.x, destination.y);
    if (same(position, end)) return false; // advance clears an arrived waypoint
    for (std::size_t i = 0; i < tiny2d::cardinalOffsets.size(); ++i) {
        const auto offset = tiny2d::cardinalOffsets[i];
        const TileCoord from{destination.x - offset.x, destination.y - offset.y};
        // Reacquiring a field can send an off-center agent back to its own
        // center along the edge that originally admitted it in the other direction.
        if (!grid.hasEdge(from, i) && !(recentering && grid.hasEdge(destination, i ^ 1U))) continue;
        const auto start = map.tileToWorldCenter(from.x, from.y);
        if (position.x >= std::min(start.x,end.x) && position.x <= std::max(start.x,end.x) &&
            position.y >= std::min(start.y,end.y) && position.y <= std::max(start.y,end.y))
            return true;
    }
    return false;
}

void validateEnemies(const NavigationSnapshot& s, const NavigationSession& candidate) {
    const auto& world = candidate.world();
    const auto& map = world.map();
    const auto& nav = candidate.navigator();
    tiny2d::DistanceField spawnField;
    if (map.spawn() != nav.field().target()) spawnField.rebuild(nav.grid(), map.spawn());
    const auto& field = map.spawn() == nav.field().target() ? nav.field() : spawnField;
    auto sites = navigationSpawnSites(world, field);
    const auto count = std::min(static_cast<std::size_t>(s.requestedEnemies), sites.cells.size());
    require(s.spawnMinimumDistance == sites.minimumDistance, "spawnMinimumDistance differs from spawn field");
    require(s.enemies.size() == count, "enemies count differs from reachable spawn budget");
    auto spawnStream = initialSpawnRng(s.world.seed);
    require(s.spawnRng == spawnStream + navigationRngIncrement * count, "spawnRng differs from slot count");

    for (std::size_t i = 0; i < count; ++i) {
        const auto& enemy = s.enemies[i];
        require(enemy.slot == i && enemy.epoch == world.epoch(), "enemy slot or epoch identity is invalid");
        require(canOccupyTiles(map, enemy.position, candidate.enemyHalfSize()), "enemy AABB is illegal");
        const auto cell = map.worldToTile(enemy.position);
        if (world.tick() == 0) {
            // Verify the seeded identities at tick zero against the pool. The
            // restored enemy vector always comes verbatim from the snapshot.
            const auto pick = i + drawNavigationIndex(spawnStream, static_cast<unsigned>(sites.cells.size() - i));
            std::swap(sites.cells[i], sites.cells[pick]);
            const auto birth = sites.cells[i];
            require(!enemy.waypoint && same(enemy.position, map.tileToWorldCenter(birth.x,birth.y)),
                    "enemy tick-zero position or waypoint differs from seeded slot");
        } else if (enemy.waypoint) {
            require(nav.field().distance(cell) >= 0 &&
                    onLeg(map, nav.grid(), enemy.position, *enemy.waypoint, cell == *enemy.waypoint), "enemy committed waypoint is illegal");
            if (same(enemy.position, map.tileToWorldCenter(cell.x, cell.y))) {
                // At extreme map scales binary32 can round a full stride back
                // to the start center. Otherwise advance leaves that center
                // on the same tick as the commitment.
                auto probe = enemy;
                nav.advance(map, probe, static_cast<float>(candidate.upgrades().enemySpeed() * tiny2d::FixedTimestep::step));
                require(same(probe.position, enemy.position), "enemy committed a leg without leaving its center");
            }
        } else if (!same(enemy.position, map.tileToWorldCenter(cell.x,cell.y))) {
            // If the player moves into a cell with no center clearance the BFS
            // can become unreachable. advance clears the leg at its actual position.
            require(nav.field().distance(cell) < 0 && onLeg(map, nav.grid(), enemy.position, cell, true),
                    "enemy off-center without a committed or unreachable leg");
        }
    }
}

void validateHealth(const NavigationSnapshot& s, const UpgradeSystem& upgrades) {
    require(s.hp >= 0 && s.hp <= upgrades.maxHP(), "hp outside current maximum");
    const auto hits = static_cast<unsigned>(upgrades.maxHP() - s.hp);
    if (hits == 0) {
        require(!s.lastDamageSlot && s.vulnerableAt == 0, "undamaged session has damage history");
    } else {
        require(s.lastDamageSlot && *s.lastDamageSlot < s.enemies.size(), "lastDamageSlot is missing or stale");
        // A Guard choice can extend future hits without changing vulnerableAt
        // for the most recent hit. Find a possible impact tick/guard level.
        bool possible = false;
        for (int level = 0; level <= upgrades.level(UpgradeId::Guard); ++level) {
            const unsigned guard = 36 + 12 * level;
            if (s.vulnerableAt < guard) continue;
            const auto impact = s.vulnerableAt - guard;
            if (impact < 1 + 36 * (hits - 1) || impact > s.world.tick) continue;
            // Each level requires a same-tick choice *after* the offer tick's contact.
            const auto chosen = s.upgrades.lastChoice ? s.upgrades.lastChoice->serial : 0;
            const auto before = std::min(chosen, (impact - 1) / NavigationSession::offerInterval);
            const auto after = chosen - before;
            const auto guardSince = unsigned(upgrades.level(UpgradeId::Guard) - level);
            if (unsigned(level) > before || guardSince > after) continue;
            if (after && (s.upgrades.lastChoice->id == UpgradeId::Guard ? !guardSince : guardSince == after))
                continue;
            if (s.hp == 0 && (impact != s.world.tick || guard != unsigned(upgrades.guardTicks()))) continue;
            if (impact == s.world.tick) {
                const auto first = std::find_if(s.enemies.begin(), s.enemies.end(), [&](const auto& enemy) {
                    return overlapsPlayer(enemy.position, s.world.movement.position);
                });
                if (first == s.enemies.end() || first->slot != *s.lastDamageSlot) continue;
            }
            possible = true;
        }
        require(possible, "vulnerableAt, hp and lastDamageSlot disagree with contact history");
    }
    if (s.world.tick && s.world.tick >= s.vulnerableAt) {
        for (const auto& enemy : s.enemies)
            require(!overlapsPlayer(enemy.position, s.world.movement.position), "vulnerable player has unresolved contact");
    }
}

void validateProgress(const NavigationSnapshot& s) {
    const auto tick = s.world.tick;
    const bool terminal = tick == NavigationSession::durationTicks || s.hp == 0;
    const auto offersDue = (terminal && tick > 0 ? tick - 1 : tick) / NavigationSession::offerInterval;
    require(s.nextOfferTick == (offersDue + 1) * NavigationSession::offerInterval,
            "nextOfferTick differs from production schedule");
    const auto& u = s.upgrades;
    require(u.serial == offersDue, "upgrade serial differs from schedule");
    if (u.offer)
        require(!terminal && tick > 0 && tick % NavigationSession::offerInterval == 0,
                "pending offer outside its exact scheduling tick");
    const auto expected = s.hp == 0 ? NavigationPhase::GameOver :
                          tick == NavigationSession::durationTicks ? NavigationPhase::Victory :
                          u.offer ? NavigationPhase::AwaitingUpgrade : NavigationPhase::Playing;
    require(s.phase == expected, "phase differs from HP, duration, or pending offer");
    int chosen = 0;
    for (auto id : upgradePool) {
        const auto level = u.levels[static_cast<unsigned>(id)];
        require(level >= 0 && level <= UpgradeSystem::levelCap(id), "upgrade level outside cap");
        chosen += level;
    }
    require(static_cast<std::uint64_t>(chosen + (u.offer ? 1 : 0)) == u.serial,
            "upgrade levels differ from issued and consumed offers");
    require(bool(u.lastChoice) == (chosen != 0), "lastChoice presence differs from consumed offers");
    if (u.lastChoice) {
        const auto& c = *u.lastChoice;
        const auto id = static_cast<unsigned>(c.id);
        require(c.epoch == s.world.epoch && c.serial == static_cast<unsigned>(chosen) &&
                id < u.levels.size() && u.levels[id] > 0, "lastChoice identity or level is invalid");
    }
    // At most five offers can issue in a 3600-tick session. With caps 4/4/3/3
    // at least three eligible IDs remain on every draw, including the fifth.
    require(u.rng == initialUpgradeRng(s.world.seed) + (3 * u.serial) * navigationRngIncrement,
            "upgrade RNG differs from issued offers");
    if (u.offer) {
        const auto& o = *u.offer;
        require(o.epoch == s.world.epoch && o.serial == u.serial && o.count == 3, "pending offer identity or count");
        std::array<bool, 4> seen{};
        for (auto id : o.choices) {
            const auto index = static_cast<unsigned>(id);
            require(index < seen.size() && !seen[index] && u.levels[index] < UpgradeSystem::levelCap(id),
                    "pending offer contains duplicate, unknown or capped ID");
            seen[index] = true;
        }
    }
}
} // namespace

UpgradeSystem::UpgradeSystem(const UpgradeSnapshot& s)
    : levels_(s.levels), rng_(s.rng), serial_(s.serial), offer_(s.offer), lastChoice_(s.lastChoice) {}

NavigationSnapshot NavigationSession::captureSnapshot() const {
    return {world_.captureSnapshot(), phase_, hp_, vulnerableAt_, nextOfferTick_, requestedEnemies_,
            spawnMinimumDistance_, lastDamageSlot_, spawnRng_, enemies_,
            {upgrades_.levels_, upgrades_.rng_, upgrades_.serial_, upgrades_.offer_, upgrades_.lastChoice_}};
}

NavigationSession::NavigationSession(tiny2d::MapSession&& world, const NavigationSnapshot& s)
    : world_(std::move(world)), enemies_(s.enemies), upgrades_(s.upgrades), spawnRng_(s.spawnRng),
      requestedEnemies_(s.requestedEnemies), spawnMinimumDistance_(s.spawnMinimumDistance), hp_(s.hp),
      vulnerableAt_(s.vulnerableAt), nextOfferTick_(s.nextOfferTick), phase_(s.phase),
      lastDamageSlot_(s.lastDamageSlot) {}

NavigationSession NavigationSession::fromSnapshot(const NavigationSnapshot& s, RestoreTimings* timings) {
    const auto begin = Clock::now();
    require(s.world.tick <= durationTicks, "world.tick exceeds session duration");
    require(s.requestedEnemies >= 0 && s.requestedEnemies <= maxEnemies &&
            s.enemies.size() <= static_cast<std::size_t>(s.requestedEnemies), "enemy population outside budget");
    validateProgress(s);
    NavigationSession candidate(tiny2d::MapSession::fromSnapshot(s.world), s);
    candidate.upgrades_.validateSeededChoices(s.world.seed);
    validateHealth(s, candidate.upgrades_);
    const auto build = Clock::now();
    const auto& map = candidate.world_.map();
    candidate.navigator_.refresh(map, s.world.epoch, enemyHalfSize(), map.worldToTile(s.world.movement.position));
    const auto rebuilt = Clock::now();
    validateEnemies(s, candidate);
    if (timings) {
        timings->cacheRebuildUs = Microseconds(rebuilt - build).count();
        timings->semanticValidationUs = Microseconds(build - begin + (Clock::now() - rebuilt)).count();
    }
    return candidate;
}

void NavigationSession::restoreSnapshot(const NavigationSnapshot& s) {
    auto candidate = fromSnapshot(s);
    static_assert(std::is_nothrow_move_assignable_v<NavigationSession>);
    *this = std::move(candidate);
}

} // namespace tiny2d_game
