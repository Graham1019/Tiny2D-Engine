#include "M5Test.h"
#include "tiny2d/FixedTimestep.h"
#include "tiny2d/MapSession.h"

#include <cmath>
#include <cstdint>
#include <vector>

using namespace tiny2d;

namespace {

struct Sample {
    std::uint64_t tick, epoch;
    std::uint32_t seed;
    Vector2 position;
    bool paused, hitX, hitY;
    std::size_t queries;
};

Sample sample(const MapSession& session) {
    const auto& move = session.collision();
    return {session.tick(), session.epoch(), session.seed(), session.playerPosition(),
            session.paused(), move.hitX, move.hitY, move.tilesQueried};
}

bool operator==(const Sample& a, const Sample& b) {
    return a.tick == b.tick && a.epoch == b.epoch && a.seed == b.seed &&
        a.position.x == b.position.x && a.position.y == b.position.y &&
        a.paused == b.paused && a.hitX == b.hitX && a.hitY == b.hitY && a.queries == b.queries;
}

std::vector<Sample> simulate(int renderHz, bool inspect) {
    FixedTimestep timing;
    MapSession session(71);
    std::vector<Sample> trace;
    std::size_t hits = 0;
    for (int frame = 0; frame < 6*renderHz; ++frame) {
        const unsigned ticks = timing.advance(1./renderHz);
        for (unsigned i = 0; i < ticks; ++i) {
            session.fixedUpdate(mapWalkInput(session.tick()));
            const auto p = session.playerPosition();
            check(canOccupyTiles(session.map(),p,session.playerHalfSize()), "script player stays legal");
            hits += session.collision().hitX || session.collision().hitY;
            trace.push_back(sample(session));
        }
        if (inspect) {
            const auto& world = session.map();
            const auto tile = world.worldToTile(session.playerPosition());
            check(world.isWalkable(tile.x,tile.y), "read-only player lookup");
            check(session.connectivity().reachableCount == world.walkableCount(), "read-only BFS lookup");
            // Vary the view/inspection frequency independently of the tick count.
            for (int x = 0; x < world.width(); ++x)
                check(world.tile(x,0) == Tile::Wall, "read-only tile row");
        }
    }
    check(trace.size() == 360 && hits > 0 && hits < trace.size(), "script moves and collides");
    return trace;
}

void commandsAndRollback() {
    MapSession session(987);
    const auto initial = sample(session);
    const auto cells = session.map().cells();
    const auto spawn = session.map().spawn();
    const auto exit = session.map().exit();
    session.fixedUpdate({1,1});
    auto p = session.playerPosition();
    check(std::abs(std::hypot(p.x-initial.position.x,p.y-initial.position.y)-2.) < 0.0002,
          "diagonal displacement is two world units");
    session.setPaused(true);
    const auto paused = sample(session);
    for (int i = 0; i < 5; ++i) session.fixedUpdate({1,0});
    check(sample(session) == paused, "pause freezes complete tick state");
    session.step({-1,0});
    check(session.tick() == paused.tick+1 && session.paused(), "single step preserves pause");
    const auto before = sample(session);
    rejects([&] { session.step({0,-2}); });
    check(sample(session) == before, "invalid input rejected atomically");

    const auto oldGeneration = session.generationUs();
    const auto oldValidation = session.validationUs();
    const auto oldFixed = session.fixedUpdateUs();
    const auto oldCollision = session.collisionUs();
    auto invalid = session.parameters();
    invalid.width = 1000000;
    rejects([&] { session.regenerate(11,invalid); });
    auto impossiblePlayer = session.parameters();
    impossiblePlayer.tileSize = 0.001f;
    rejects([&] { session.regenerate(11,impossiblePlayer); });
    check(sample(session) == before && session.map().cells() == cells &&
          session.map().spawn() == spawn && session.map().exit() == exit &&
          session.generationUs() == oldGeneration && session.validationUs() == oldValidation &&
          session.fixedUpdateUs() == oldFixed && session.collisionUs() == oldCollision,
          "failed parameters or invalid spawn preserve previous map, state and observations");
    session.reset();
    check(session.tick() == 0 && session.paused() && session.epoch() == initial.epoch+1,
          "reset starts a new epoch at zero and preserves pause");
    p = session.playerPosition();
    check(session.seed() == initial.seed && cells == session.map().cells() &&
          p.x == initial.position.x && p.y == initial.position.y &&
          session.map().exit() == exit && session.map().spawn() == spawn &&
          !session.collision().hitX && !session.collision().hitY && session.collision().tilesQueried == 0,
          "reset regenerates identical map and spawn and clears contacts");
    session.setPaused(false);
    session.fixedUpdate({0,1});
    session.regenerate(71);
    check(session.epoch() == initial.epoch+2 && session.tick() == 0 && session.seed() == 71 &&
          !session.paused(), "regenerate resets seed and tick in new epoch");
    check(session.generationUs() >= 0 && session.validationUs() >= 0 &&
          session.fixedUpdateUs() == 0 && session.collisionUs() == 0, "timing scopes reset");
}

void rebuilds() {
    MapSession session;
    for (std::uint32_t seed = 0; seed < 80; ++seed) {
        auto config = session.parameters();
        config.width = seed % 2 ? 13 : 64;
        config.height = seed % 3 ? 10 : 40;
        const auto oldEpoch = session.epoch();
        session.regenerate(seed, config);
        const auto cell = session.map().worldToTile(session.playerPosition());
        check(session.epoch() == oldEpoch+1 && session.map().spawn() == cell &&
              session.map().width() == config.width && session.map().height() == config.height &&
              session.connectivity().connected, "replacement owns current geometry, spawn and validation");
        session.fixedUpdate(mapWalkInput(0));
        check(session.tick() == 1 && session.fixedUpdateUs() >= session.collisionUs(), "new epoch advances");
    }
}

} // namespace

int main() {
    try {
        const auto sixty = simulate(60,false);
        check(simulate(30,true) == sixty && simulate(120,true) == sixty &&
              simulate(60,true) == sixty, "identical every-tick state at 30/60/120 Hz");
        commandsAndRollback();
        rebuilds();
        std::cout << "360 per-tick samples at 30/60/120 Hz; 80 replacement epochs\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
