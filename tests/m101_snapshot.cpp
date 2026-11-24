#include "M5Test.h"
#include "tiny2d_game/NavigationSession.h"
#include "tiny2d_game/SnapshotCodec.h"

#include <algorithm>
#include <limits>

using namespace tiny2d;
using namespace tiny2d_game;

namespace {
void same(const NavigationSession& a, const NavigationSession& b) {
    const auto left = a.captureSnapshot(), right = b.captureSnapshot();
    const auto difference = firstDifference(left, right);
    if (!difference.empty()) throw std::runtime_error("authority differs at " + difference);
    check(encodeSnapshot(left) == encodeSnapshot(right), "canonical bytes differ");
}

NavigationSession roundTrip(const NavigationSession& original) {
    const auto bytes = encodeSnapshot(original.captureSnapshot());
    RestoreTimings timings;
    auto restored = NavigationSession::fromSnapshot(decodeSnapshot(bytes), &timings);
    same(original, restored);
    const auto& nav = restored.navigator();
    const auto& world = restored.world();
    check(nav.stats().graphBuildCount == 1 && nav.stats().fieldRebuildCount == 1 &&
          nav.stats().cacheReuseCount == 0 &&
          nav.field().target() == world.map().worldToTile(world.playerPosition()),
          "fresh cache at the restored target");
    return restored;
}

void chooseFirst(NavigationSession& s) {
    const auto o = *s.upgrades().offer();
    check(s.choose(o.epoch, o.serial, o.choices[0]), "choice must apply");
}

void continuation() {
    NavigationSession original;
    roundTrip(original);
    bool hit = false, collision = false;
    unsigned continuations = 0;
    for (unsigned i = 0; i < 25; ++i) original.step(mapWalkInput(i));
    check(std::any_of(original.enemies().begin(), original.enemies().end(),
                      [](const auto& enemy) { return enemy.waypoint.has_value(); }), "fixture is mid-leg");
    auto restored = roundTrip(original);
    while (original.phase() != NavigationPhase::GameOver) {
        const auto input = mapWalkInput(original.world().tick());
        const auto hp = original.hp();
        if (original.phase() == NavigationPhase::AwaitingUpgrade) {
            roundTrip(original);
            chooseFirst(original);
            chooseFirst(restored);
        } else {
            original.step(input);
            restored.step(input);
            ++continuations;
        }
        same(original, restored);
        if (!hit && original.hp() < hp) {
            hit = original.world().tick() < original.vulnerableAt() && original.lastDamageSlot().has_value();
            roundTrip(original);
        }
        if (!collision && (original.world().collision().hitX || original.world().collision().hitY)) {
            collision = true;
            roundTrip(original);
        }
    }
    check(hit && collision && continuations > 600 && original.world().tick() == 702,
          "covered cooldown, collision, long continuation, and accepted game over");
    roundTrip(original);
}

void controlBoundaries() {
    NavigationSession s(71,{},0);
    while (s.world().tick() < 600) s.step();
    check(s.phase() == NavigationPhase::AwaitingUpgrade, "pending offer fixture");
    s.setPaused(true);
    auto mirror = roundTrip(s);
    const auto offer = *s.upgrades().offer();
    for (auto* session : {&s, &mirror})
        check(session->choose(offer.epoch, offer.serial, UpgradeId::Vitality) &&
              session->world().tick() == 600 && session->world().paused() && session->hp() == 6,
              "same tick Vitality choice and manual pause");
    same(s,mirror);
    roundTrip(s);
    s.fixedUpdate();
    check(s.world().tick() == 600, "manual pause survived choice");
    s.step();
    roundTrip(s);
    s.setPaused(false);
    while (s.world().tick() < NavigationSession::durationTicks) {
        if (s.upgrades().offer()) {
            // Cover all four level channels using actual offered choices.
            const auto o = *s.upgrades().offer();
            auto pick = std::min_element(o.choices.begin(), o.choices.begin()+o.count, [&](auto a, auto b) {
                return s.upgrades().level(a) < s.upgrades().level(b);
            });
            check(s.choose(o.epoch, o.serial, *pick), "select least-used upgrade");
            roundTrip(s);
        }
        s.step();
    }
    check(s.phase() == NavigationPhase::Victory, "victory fixture");
    for (auto id : upgradePool) check(s.upgrades().level(id) != 0, "covered each upgrade level field");
    roundTrip(s);
    s.reset();
    s.reset();
    s.regenerate(83, MapParameters{33,27,16,2,3});
    check(s.world().epoch() == 4 && !s.upgrades().lastChoice(), "epoch replacement fixture");
    roundTrip(s);

    for (auto p : {MapParameters{128,96}, MapParameters{5,5}}) {
        NavigationSession crowded(19, p, 256);
        roundTrip(crowded);
        for (int i = 0; i < 21; ++i) crowded.step(mapWalkInput(i));
        roundTrip(crowded);
    }
}

void nonstandardTileSizes() {
    // This map has an occupiable player AABB whose tile center lacks
    // clearance. An unreachable BFS can clear an enemy's leg mid-segment.
    NavigationSession narrow(5,MapParameters{41,29,3.f,12,3},16);
    while (narrow.world().tick() < 20) narrow.step(mapWalkInput(narrow.world().tick()));
    check(!narrow.navigator().field().visited(), "fixture hits unreachable field");
    auto restored = roundTrip(narrow);
    for (int i = 0; i < 80; ++i) {
        const auto input = mapWalkInput(narrow.world().tick());
        narrow.step(input);
        restored.step(input);
        same(narrow, restored);
    }

    NavigationSession large(71, MapParameters{17,13,1e10f},16);
    large.step();
    const auto& e = large.enemies().front();
    check(e.waypoint.has_value(), "large tile retains precision-stalled commitment");
    roundTrip(large);
}

// Use one live session throughout. Both the canonical bytes and existing
// derived cache must survive every rejected import, including same-epoch ones.
template<class F>
void rejectEdit(NavigationSession& live, const NavigationSnapshot& source, F edit) {
    auto damaged = source;
    edit(damaged);
    const auto before = encodeSnapshot(live.captureSnapshot());
    const auto builds = live.navigator().stats().graphBuildCount;
    rejects([&] { live.restoreSnapshot(damaged); });
    check(before == encodeSnapshot(live.captureSnapshot()) &&
          builds == live.navigator().stats().graphBuildCount, "failed restore mutated live state");
}

void corruptions() {
    NavigationSession live;
    const auto zero = live.captureSnapshot();
    rejectEdit(live, zero, [](auto& s) { s.world.spawn = s.world.exit; });
    rejectEdit(live, zero, [](auto& s) { s.world.tiles[0] = Tile::Floor; });
    rejectEdit(live, zero, [](auto& s) { s.world.tiles[0] = static_cast<Tile>(2); });
    rejectEdit(live, zero, [](auto& s) { s.world.epoch = 0; });
    rejectEdit(live, zero, [](auto& s) { s.world.fingerprint ^= 1; });
    rejectEdit(live, zero, [](auto& s) { s.world.movement.hitX = true; });
    rejectEdit(live, zero, [](auto& s) { s.world.movement.position.x += 1; });
    rejectEdit(live, zero, [](auto& s) { s.world.parameters.extraConnections = 257; });
    rejectEdit(live, zero, [](auto& s) { s.world.parameters.tileSize = std::numeric_limits<float>::quiet_NaN(); });
    rejectEdit(live, zero, [](auto& s) { s.spawnMinimumDistance--; });
    rejectEdit(live, zero, [](auto& s) { s.spawnRng++; });
    rejectEdit(live, zero, [](auto& s) { s.enemies.pop_back(); });
    rejectEdit(live, zero, [](auto& s) { s.enemies[0].position = s.enemies[1].position; });
    rejectEdit(live, zero, [](auto& s) { s.upgrades.rng++; });
    for (int i = 0; i < 7; ++i) live.step(mapWalkInput(i));
    const auto mid = live.captureSnapshot();
    rejectEdit(live, mid, [](auto& s) { s.world.tick = 3601; });
    rejectEdit(live, mid, [](auto& s) { s.world.movement.tilesQueried = 0; });
    rejectEdit(live, mid, [](auto& s) { s.world.movement.position = {0,0}; });
    rejectEdit(live, mid, [](auto& s) { s.enemies[1].slot = 0; });
    rejectEdit(live, mid, [](auto& s) { s.enemies[1].epoch++; });
    rejectEdit(live, mid, [](auto& s) { s.enemies[0].waypoint.reset(); });
    rejectEdit(live, mid, [](auto& s) { s.enemies[0].waypoint = TileCoord{-1,-1}; });
    rejectEdit(live, mid, [](auto& s) { s.enemies[0].position.x += .25f; s.enemies[0].position.y += .25f; });
    rejectEdit(live, mid, [](auto& s) {
        const auto w = *s.enemies[0].waypoint;
        s.enemies[0].position = {float((w.x + .5)*s.world.parameters.tileSize),
                                 float((w.y + .5)*s.world.parameters.tileSize)};
    });
    rejectEdit(live, mid, [&](auto& s) {
        auto& e = s.enemies[0];
        const auto cell = live.world().map().worldToTile(e.position);
        check(cell != *e.waypoint, "fixture before tile midpoint");
        e.position = live.world().map().tileToWorldCenter(cell.x,cell.y);
    });
    rejectEdit(live, mid, [](auto& s) { s.requestedEnemies = 257; });
    rejectEdit(live, mid, [](auto& s) { s.phase = NavigationPhase::GameOver; });
    rejectEdit(live, mid, [](auto& s) { s.nextOfferTick = 1200; });
    rejectEdit(live, mid, [](auto& s) { s.vulnerableAt = 10; });
    rejectEdit(live, mid, [](auto& s) { s.hp = 6; });
    rejectEdit(live, mid, [](auto& s) { s.hp = -1; });
    rejectEdit(live, mid, [](auto& s) { s.hp--; s.lastDamageSlot = 0; s.vulnerableAt = 1000; });

    NavigationSession peaceful(71,{},0);
    while (peaceful.world().tick() < 600) peaceful.step();
    const auto pending = peaceful.captureSnapshot();
    rejectEdit(live, pending, [](auto& s) { s.world.tick++; });
    rejectEdit(live, pending, [](auto& s) { s.phase = NavigationPhase::Playing; });
    rejectEdit(live, pending, [](auto& s) { s.upgrades.offer->serial++; });
    rejectEdit(live, pending, [](auto& s) { s.upgrades.offer->count--; });
    rejectEdit(live, pending, [](auto& s) { s.upgrades.offer->choices[0] = s.upgrades.offer->choices[1]; });
    rejectEdit(live, pending, [](auto& s) { s.upgrades.offer->choices[0] = static_cast<UpgradeId>(7); });
    rejectEdit(live, pending, [](auto& s) { std::swap(s.upgrades.offer->choices[0], s.upgrades.offer->choices[1]); });
    chooseFirst(peaceful);
    const auto chosen = peaceful.captureSnapshot();
    rejectEdit(live, chosen, [](auto& s) { s.upgrades.lastChoice.reset(); });
    rejectEdit(live, chosen, [](auto& s) { s.upgrades.lastChoice->epoch++; });
    rejectEdit(live, chosen, [](auto& s) { s.upgrades.lastChoice->serial++; });
    rejectEdit(live, chosen, [](auto& s) { s.upgrades.levels[0] = 5; });
    rejectEdit(live, chosen, [](auto& s) {
        s.upgrades.levels = {0,0,0,1};
        s.upgrades.lastChoice->id = UpgradeId::Guard; // Seed 71 never offered Guard at serial 1.
        s.hp = 5;
    });
    live.restoreSnapshot(pending); // same logical epoch, different world and population
    check(live.enemies().empty() && live.navigator().stats().cacheReuseCount == 0 &&
          encodeSnapshot(live.captureSnapshot()) == encodeSnapshot(pending), "same-epoch replacement");
}
}

int main() {
    try {
        continuation();
        controlBoundaries();
        nonstandardTileSizes();
        corruptions();
        std::cout << "snapshot authority, mid-leg, cooldown, same-tick choice, terminal states, "
                     "fresh caches, 0/16/256 population and transactional rejection passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
