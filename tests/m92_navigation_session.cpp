#include "M5Test.h"
#include "tiny2d/FixedTimestep.h"
#include "tiny2d_game/NavigationSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>

using namespace tiny2d;
using namespace tiny2d_game;

namespace tiny2d_game {
// Put two coincident enemies into the otherwise empty late-game fixture.
// This makes terminal/offer priority observable on the exact boundary tick.
struct NavigationSessionTestAccess {
    static void lethalContact(NavigationSession& session) {
        session.hp_ = 1;
        for (std::size_t slot = 0; slot < 2; ++slot) {
            NavigationEnemy enemy;
            enemy.position = session.world_.playerPosition();
            enemy.epoch = session.world_.epoch();
            enemy.slot = slot;
            session.enemies_.push_back(enemy);
        }
    }
};
}

namespace {

bool equalPosition(Vector2 a, Vector2 b) { return a.x == b.x && a.y == b.y; }

// Compare every field affecting future simulation; map cells and fixed rules
// are also checked. The optional flag compares reset/replay across new epochs.
void sameAuthority(const NavigationSession& a, const NavigationSession& b, bool allowNewEpoch = false) {
    const auto& x = a.world();
    const auto& y = b.world();
    const auto& p = x.parameters();
    const auto& q = y.parameters();
    check(p.width == q.width && p.height == q.height && p.roomCount == q.roomCount &&
          p.extraConnections == q.extraConnections && p.tileSize == q.tileSize, "map parameters");
    check(x.seed() == y.seed() && x.map().cells() == y.map().cells() &&
          x.map().spawn() == y.map().spawn() && x.map().exit() == y.map().exit() &&
          (allowNewEpoch || x.epoch() == y.epoch()), "map identity and cells");
    check(x.tick() == y.tick() && x.paused() == y.paused() &&
          equalPosition(x.playerPosition(),y.playerPosition()) && x.collision().hitX == y.collision().hitX &&
          x.collision().hitY == y.collision().hitY && x.collision().tilesQueried == y.collision().tilesQueried,
          "map tick, pause, player and collision");
    check(a.phase() == b.phase() && a.hp() == b.hp() && a.vulnerableAt() == b.vulnerableAt() &&
          a.lastDamageSlot() == b.lastDamageSlot() && a.nextOfferTick() == b.nextOfferTick() &&
          a.spawnRngState() == b.spawnRngState() && a.requestedEnemies() == b.requestedEnemies() &&
          a.enemies().size() == b.enemies().size() && a.spawnMinimumDistance() == b.spawnMinimumDistance(),
          "game phase, cooldown, RNG and population");
    for (std::size_t i = 0; i < a.enemies().size(); ++i) {
        const auto& e = a.enemies()[i];
        const auto& f = b.enemies()[i];
        check(e.epoch == x.epoch() && f.epoch == y.epoch() && e.slot == f.slot &&
              equalPosition(e.position,f.position) && e.waypoint == f.waypoint, "enemy and committed leg");
    }
    const auto& u = a.upgrades();
    const auto& v = b.upgrades();
    for (auto id : upgradePool) check(u.level(id) == v.level(id), "all upgrade levels");
    check(u.rngState() == v.rngState() && u.serial() == v.serial() && u.playerSpeed() == v.playerSpeed() &&
          u.enemySpeed() == v.enemySpeed() && u.maxHP() == v.maxHP() && u.guardTicks() == v.guardTicks() &&
          u.offer().has_value() == v.offer().has_value() && u.lastChoice().has_value() == v.lastChoice().has_value(),
          "upgrade RNG, serial, offer, choice and attributes");
    if (const auto& o = u.offer())
        check(o->epoch == x.epoch() && v.offer()->epoch == y.epoch() && o->serial == v.offer()->serial &&
              o->count == v.offer()->count && o->choices == v.offer()->choices, "pending offer identity and options");
    if (const auto& c = u.lastChoice())
        check(c->epoch == x.epoch() && v.lastChoice()->epoch == y.epoch() && c->serial == v.lastChoice()->serial &&
              c->id == v.lastChoice()->id, "last control choice identity");
}

void selectFirst(NavigationSession& s) {
    const auto& offer = s.upgrades().offer();
    if (offer) check(s.choose(offer->epoch,offer->serial,offer->choices[0]), "choose first candidate");
}

struct ScheduledRun {
    int hz;
    NavigationSession session;
    FixedTimestep clock;
    unsigned frames = 0, due = 0;

    void nextTick() {
        while (due == 0) {
            ++frames;
            due = clock.advance(1./hz);
            // Every rendered frame may inspect the same field, including frames without a tick.
            (void)session.navigator().next(session.world().map().spawn());
            for (auto id : upgradePool) (void)session.upgrades().eligible(id);
        }
        session.fixedUpdate(mapWalkInput(session.world().tick()));
        --due;
    }

    void choose() {
        selectFirst(session);
        clock.reset();
        due = 0;
    }
};

void schedule(std::uint32_t seed, int count, const MapParameters& parameters) {
    std::array<ScheduledRun,3> runs{{
        {30,NavigationSession(seed,parameters,count),{}},
        {60,NavigationSession(seed,parameters,count),{}},
        {120,NavigationSession(seed,parameters,count),{}}
    }};
    const auto& ref = runs[1].session;
    unsigned draws = 0;
    while (ref.phase() == NavigationPhase::Playing || ref.phase() == NavigationPhase::AwaitingUpgrade) {
        if (ref.phase() == NavigationPhase::AwaitingUpgrade) {
            const auto before = ref.world().tick();
            for (auto& run : runs) run.choose();
            check(ref.world().tick() == before, "upgrade is a same-tick control event");
            sameAuthority(ref,runs[0].session);
            sameAuthority(ref,runs[2].session);
            ++draws;
        }
        for (auto& run : runs) run.nextTick();
        sameAuthority(ref,runs[0].session);
        sameAuthority(ref,runs[2].session);
    }
    if (count == 0)
        check(draws == 5 && ref.world().tick() == 3600 && ref.phase() == NavigationPhase::Victory &&
              !ref.upgrades().offer() && runs[0].frames == 1800 && runs[1].frames == 3600 &&
              runs[2].frames == 7200, "30/60/120 schedules reach victory before sixth offer");
    runs[0].session.fixedUpdate({9,0});
    runs[0].session.step({1,1});
    sameAuthority(ref,runs[0].session);
}

void populationAndReplacement() {
    rejects([] { NavigationSession s(71,{},257); });
    rejects([] { NavigationSession s(71,{},-1); });
    NavigationSession s(71,{},256), baseline = s;
    check(s.enemies().size() == 256 && !*s.spawnShortfallReason(), "full stress load");
    std::set<std::pair<int,int>> tiles;
    for (std::size_t i = 0; i < s.enemies().size(); ++i) {
        const auto& e = s.enemies()[i];
        const auto tile = s.world().map().worldToTile(e.position);
        check(e.slot == i && e.epoch == s.world().epoch() && !e.waypoint &&
              s.navigator().field().distance(tile) >= s.spawnMinimumDistance() &&
              canOccupyTiles(s.world().map(),e.position,s.enemyHalfSize()) &&
              tiles.emplace(tile.x,tile.y).second, "stable distinct reachable legal spawns");
    }
    rejects([&] { s.step({7,0}); });
    sameAuthority(s,baseline);
    s.setPaused(true);
    s.fixedUpdate({-1,1});
    check(s.world().tick() == 0, "manual pause freezes playing state");
    s.step({1,0});
    check(s.world().tick() == 1 && s.world().paused(), "single step while manually paused");
    s.reset();
    baseline.setPaused(true);
    sameAuthority(s,baseline,true);
    check(s.world().epoch() == baseline.world().epoch()+1 && s.navigator().stats().fieldRebuildCount == 1,
          "reset starts new epoch with one field");
    for (unsigned i = 0; i < 12; ++i) {
        auto p = s.world().parameters();
        p.width = i % 2 ? 64 : 5;
        p.height = i % 2 ? 40 : 5;
        const auto old = s.world().epoch();
        s.regenerate(i,p);
        check(s.world().epoch() == old+1 && s.world().paused() && s.world().seed() == i &&
              s.world().tick() == 0 && s.navigator().stats().fieldRebuildCount == 1, "fresh epoch and cache");
        if (p.width == 5) check(s.enemies().size() < 256 && *s.spawnShortfallReason(), "bounded shortfall");
        for (const auto& e : s.enemies()) check(e.epoch == old+1 && !e.waypoint, "old waypoints invalidated");
        auto unchanged = s;
        p.tileSize = .001f;
        rejects([&] { s.regenerate(i+5,p); });
        sameAuthority(s,unchanged);
    }
}

void offerBoundaries() {
    NavigationSession s(71,{},16), other(71,{},0);
    check(s.spawnRngState() != other.spawnRngState() &&
          s.upgrades().rngState() == other.upgrades().rngState(), "spawn RNG is independent");
    for (unsigned i = 0; i < 600; ++i) { s.step(); other.step(); }
    check(s.phase() == NavigationPhase::AwaitingUpgrade && s.world().tick() == 600 &&
          s.upgrades().offer()->choices == other.upgrades().offer()->choices &&
          s.upgrades().rngState() == other.upgrades().rngState(), "spawn count does not affect upgrade draws");
    const auto offer = *s.upgrades().offer();
    s.setPaused(true);
    const auto frozen = s;
    for (int i = 0; i < 30; ++i) {
        s.step({1,1});
        s.fixedUpdate({-1,0});
        (void)s.navigator().next(s.world().map().spawn());
    }
    sameAuthority(s,frozen);
    check(s.navigator().stats().cacheReuseCount == frozen.navigator().stats().cacheReuseCount &&
          s.navigator().stats().fieldRebuildCount == 1, "frozen control and debug queries never refresh field");
    check(!s.choose(offer.epoch+1,offer.serial,offer.choices[0]) &&
          !s.choose(offer.epoch,offer.serial-1,offer.choices[0]) &&
          !s.choose(offer.epoch,offer.serial,UpgradeId::Guard), "invalid choices rejected atomically");
    sameAuthority(s,frozen);
    auto badParams = s.world().parameters();
    badParams.width = 0;
    rejects([&] { s.regenerate(7,badParams); });
    sameAuthority(s,frozen);
    check(s.choose(offer.epoch,offer.serial,UpgradeId::Vitality) && s.hp() == frozen.hp()+1 &&
          s.upgrades().maxHP() == 6 && s.world().paused() && s.world().tick() == 600 &&
          !s.choose(offer.epoch,offer.serial,UpgradeId::Vitality), "vitality heals once without unpausing or ticking");
    s.fixedUpdate({1,1});
    check(s.world().tick() == 600 && s.phase() == NavigationPhase::Playing, "manual pause survives choice");
    s.reset();
    s.setPaused(false);
    for (unsigned i = 0; i < 600; ++i) s.step();
    check(s.upgrades().offer()->choices == offer.choices && s.world().epoch() == offer.epoch+1 &&
          !s.choose(offer.epoch,offer.serial,offer.choices[0]), "reset restores stream and rejects old identity");
    auto replay = frozen;
    replay.setPaused(false);
    sameAuthority(s,replay,true);
    s.regenerate(12345);
    check(s.upgrades().serial() == 0 && !s.upgrades().lastChoice() && !s.upgrades().offer() &&
          !s.choose(offer.epoch,offer.serial,offer.choices[0]) && s.hp() == 5 && s.vulnerableAt() == 0,
          "regenerate clears old offer, choice, cooldown and statistics");
}

void physicalEffects() {
    NavigationSession original(71,{},16);
    for (unsigned i=0; i<600; ++i) original.step();
    const auto o = *original.upgrades().offer();
    auto fleet = original, slow = original;
    check(fleet.choose(o.epoch,o.serial,UpgradeId::Fleet) && slow.choose(o.epoch,o.serial,UpgradeId::Slow),
          "both choices valid");
    const auto p0 = original.world().playerPosition();
    fleet.step({1,0}); slow.step({1,0});
    check(std::abs(fleet.world().playerPosition().x - p0.x - 2.2) < .0001 &&
          std::abs(slow.world().playerPosition().x - p0.x - 2.) < .0001, "fleet increases real fixed-tick displacement");
    bool slower = false;
    for (const auto& e : original.enemies()) {
        auto distance = [&](const NavigationSession& session) {
            const auto p = session.enemies()[e.slot].position;
            return std::hypot(p.x-e.position.x,p.y-e.position.y);
        };
        slower |= std::abs(distance(fleet)-1.f)<.0001 && std::abs(distance(slow)-.9f)<.0001;
    }
    check(slower, "slow changes actual enemy movement");

    bool testedGuard = false;
    for (unsigned seed=0; seed<12 && !testedGuard; ++seed) {
        NavigationSession s(seed,{},16);
        for (unsigned i=0; i<600; ++i) s.step();
        const auto offer = *s.upgrades().offer();
        if (!s.choose(offer.epoch,offer.serial,UpgradeId::Guard)) continue;
        std::uint64_t previous = 0;
        while (s.phase() == NavigationPhase::Playing) {
            const int hp = s.hp();
            s.step();
            if (s.hp() == hp) continue;
            check(s.hp() == hp-1 && s.vulnerableAt() == s.world().tick()+48,
                  "at most one contact loss, real guard cooldown");
            const auto p = s.world().playerPosition();
            const auto hit = std::find_if(s.enemies().begin(),s.enemies().end(),[&](const auto& e) {
                return std::abs(e.position.x-p.x)<10 && std::abs(e.position.y-p.y)<10;
            });
            check(hit != s.enemies().end() && s.lastDamageSlot() == hit->slot, "stable first contact slot");
            if (previous) {
                check(s.world().tick() == previous+48, "overlapping enemies cannot hit early");
                testedGuard = true;
            }
            previous = s.world().tick();
        }
        check(s.phase() == NavigationPhase::GameOver && s.hp() == 0, "lethal contact ends game");
        const auto dead = s;
        s.step(); s.fixedUpdate(); sameAuthority(s,dead);
    }
    check(testedGuard, "exercised guard across multiple hits");
}

void lethalPriority() {
    for (const int target : {600,3600}) {
        NavigationSession s(71,{},0);
        while (s.world().tick() < static_cast<unsigned>(target-1)) {
            selectFirst(s);
            s.step();
        }
        NavigationSessionTestAccess::lethalContact(s);
        s.step();
        check(s.phase() == NavigationPhase::GameOver && s.hp() == 0 && !s.upgrades().offer() &&
              s.world().tick() == static_cast<unsigned>(target) && s.lastDamageSlot() == 0,
              "lethal contact beats offer and victory at exact boundary");
    }
}

void defaultMapRegression() {
    MapSession map(71), explicitSpeed(71);
    NavigationSession session(71,{},0);
    // Golden FNV-1a trace independently captured from accepted M9.1 f791853.
    std::uint64_t hash = UINT64_C(14695981039346656037);
    auto append = [&](std::uint32_t word) {
        for (unsigned byte = 0; byte < 4; ++byte) {
            hash ^= static_cast<std::uint8_t>(word >> (8*byte));
            hash *= UINT64_C(1099511628211);
        }
    };
    for (unsigned i=0; i<599; ++i) {
        const auto input = mapWalkInput(i);
        map.step(input); explicitSpeed.step(input,120); session.step(input);
        check(equalPosition(map.playerPosition(),explicitSpeed.playerPosition()) &&
              equalPosition(map.playerPosition(),session.world().playerPosition()) &&
              map.collision().tilesQueried == session.world().collision().tilesQueried &&
              map.collision().hitX == session.world().collision().hitX && map.collision().hitY == session.world().collision().hitY &&
              map.tick() == session.world().tick(), "per-tick M9.1 default speed and collision");
        const auto position = map.playerPosition();
        std::uint32_t x, y;
        static_assert(sizeof(x) == sizeof(position.x));
        std::memcpy(&x,&position.x,sizeof(x));
        std::memcpy(&y,&position.y,sizeof(y));
        append(static_cast<std::uint32_t>(map.tick()));
        append(x); append(y);
        append(map.collision().hitX); append(map.collision().hitY);
        append(static_cast<std::uint32_t>(map.collision().tilesQueried));
    }
    check(hash == UINT64_C(0xffbdab960a0b517c), "accepted M9.1 movement trace");
    const auto before = map.playerPosition();
    for (auto speed : {-1.f,100001.f,std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::quiet_NaN()})
        rejects([&] { map.step({1,0},speed); });
    check(map.tick() == 599 && equalPosition(before,map.playerPosition()), "invalid speed rejected before movement");
}

} // namespace

int main() {
    try {
        schedule(71,0,{});
        schedule(73,16,{});
        MapParameters small; small.width=21; small.height=17;
        schedule(101,256,small);
        populationAndReplacement(); offerBoundaries(); physicalEffects(); lethalPriority(); defaultMapRegression();
        std::cout << "complete per-tick authority at 30/60/120 Hz, spawn loads 0/16/256, replacement, "
                     "four applied upgrades, cooldown, terminal precedence and map regression passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
