#include "tiny2d_game/SnapshotCodec.h"
#include "BinaryCodec.h"

namespace tiny2d_game {

// Field paths identify the first difference in the same order as the codec.
// Floats use their exact bits, including signed zero.
std::string firstDifference(const NavigationSnapshot& a, const NavigationSnapshot& b) {
#define FIELD(name) if (a.name != b.name) return #name
#define FLOAT_FIELD(name) if (binary::floatBits(a.name) != binary::floatBits(b.name)) return #name
    FIELD(world.seed);
    FIELD(world.parameters.width);
    FIELD(world.parameters.height);
    FLOAT_FIELD(world.parameters.tileSize);
    FIELD(world.parameters.roomCount);
    FIELD(world.parameters.extraConnections);
    if (a.world.tiles.size() != b.world.tiles.size()) return "world.tiles.size";
    for (std::size_t i = 0; i < a.world.tiles.size(); ++i)
        if (a.world.tiles[i] != b.world.tiles[i]) return "world.tiles[" + std::to_string(i) + "]";
    FIELD(world.spawn); FIELD(world.exit);
    FIELD(world.fingerprint); FIELD(world.epoch); FIELD(world.tick); FIELD(world.paused);
    FLOAT_FIELD(world.movement.position.x); FLOAT_FIELD(world.movement.position.y);
    FIELD(world.movement.hitX); FIELD(world.movement.hitY); FIELD(world.movement.tilesQueried);
    FIELD(phase); FIELD(hp); FIELD(vulnerableAt); FIELD(nextOfferTick); FIELD(requestedEnemies);
    FIELD(spawnMinimumDistance); FIELD(lastDamageSlot); FIELD(spawnRng);
    if (a.enemies.size() != b.enemies.size()) return "enemies.size";
    for (std::size_t i = 0; i < a.enemies.size(); ++i) {
        const auto& x = a.enemies[i];
        const auto& y = b.enemies[i];
        const char* field;
        if (x.slot != y.slot) field = "slot";
        else if (x.epoch != y.epoch) field = "epoch";
        else if (binary::floatBits(x.position.x) != binary::floatBits(y.position.x)) field = "position.x";
        else if (binary::floatBits(x.position.y) != binary::floatBits(y.position.y)) field = "position.y";
        else if (x.waypoint != y.waypoint) field = "waypoint";
        else continue;
        return "enemies[" + std::to_string(i) + "]." + field;
    }
    for (std::size_t i = 0; i < a.upgrades.levels.size(); ++i)
        if (a.upgrades.levels[i] != b.upgrades.levels[i]) return "upgrades.levels[" + std::to_string(i) + "]";
    FIELD(upgrades.rng); FIELD(upgrades.serial);
    FIELD(upgrades.offer.has_value());
    if (a.upgrades.offer) {
        FIELD(upgrades.offer->epoch); FIELD(upgrades.offer->serial); FIELD(upgrades.offer->count);
        const auto count = a.upgrades.offer->count;
        if (count < 0 || count > 3) throw std::invalid_argument("invalid offer count in comparison");
        for (int i = 0; i < count; ++i)
            if (a.upgrades.offer->choices[i] != b.upgrades.offer->choices[i])
                return "upgrades.offer.choices[" + std::to_string(i) + "]";
    }
    FIELD(upgrades.lastChoice.has_value());
    if (a.upgrades.lastChoice) {
        FIELD(upgrades.lastChoice->epoch); FIELD(upgrades.lastChoice->serial); FIELD(upgrades.lastChoice->id);
    }
#undef FLOAT_FIELD
#undef FIELD
    return {};
}
} // namespace tiny2d_game
