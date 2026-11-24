#include "tiny2d_game/SnapshotCodec.h"
#include "BinaryCodec.h"

namespace tiny2d_game {
namespace {
using namespace binary;

void writeCoord(Writer& out, tiny2d::TileCoord p) {
    out.u32(p.x);
    out.u32(p.y);
}
void writePosition(Writer& out, tiny2d::Vector2 p) {
    out.f32(p.x);
    out.f32(p.y);
}
int readInt(Reader& in) {
    const auto v = in.u32();
    if (v > static_cast<unsigned>(std::numeric_limits<int>::max()))
        throw std::invalid_argument("integer field outside range");
    return static_cast<int>(v);
}
tiny2d::TileCoord readCoord(Reader& in) { return {readInt(in), readInt(in)}; }
tiny2d::Vector2 readPosition(Reader& in) { return {in.f32(), in.f32()}; }
UpgradeId readId(Reader& in) {
    const auto id = in.u8();
    if (id >= upgradePool.size()) throw std::invalid_argument("unknown upgrade id");
    return static_cast<UpgradeId>(id);
}
Bytes payload(const NavigationSnapshot& s) {
    const auto& m = s.world;
    const auto& p = m.parameters;
    if (m.tiles.size() > tiny2d::TileMap::maxTiles || s.enemies.size() > 256)
        throw std::length_error("snapshot collection exceeds budget");
    Writer out;
    out.bytes.reserve(192 + m.tiles.size() + 29*s.enemies.size());
    out.u32(m.seed);
    out.u32(p.width);
    out.u32(p.height);
    out.f32(p.tileSize);
    out.u32(p.roomCount);
    out.u32(p.extraConnections);
    out.u32(static_cast<std::uint32_t>(m.tiles.size()));
    for (auto tile : m.tiles) out.u8(static_cast<std::uint8_t>(tile));
    writeCoord(out, m.spawn);
    writeCoord(out, m.exit);
    out.u64(m.fingerprint);
    out.u64(m.epoch);
    out.u64(m.tick);
    out.u8(m.paused);
    writePosition(out, m.movement.position);
    out.u8(m.movement.hitX);
    out.u8(m.movement.hitY);
    out.u64(m.movement.tilesQueried);
    out.u8(static_cast<std::uint8_t>(s.phase));
    out.u32(s.hp);
    out.u64(s.vulnerableAt);
    out.u64(s.nextOfferTick);
    out.u32(s.requestedEnemies);
    out.u32(s.spawnMinimumDistance);
    out.u8(s.lastDamageSlot.has_value());
    if (s.lastDamageSlot) out.u64(*s.lastDamageSlot);
    out.u64(s.spawnRng);
    out.u32(static_cast<std::uint32_t>(s.enemies.size()));
    for (const auto& e : s.enemies) {
        out.u64(e.slot);
        out.u64(e.epoch);
        writePosition(out, e.position);
        out.u8(e.waypoint.has_value());
        if (e.waypoint) writeCoord(out, *e.waypoint);
    }
    const auto& u = s.upgrades;
    for (int level : u.levels) out.u32(level);
    out.u64(u.rng);
    out.u64(u.serial);
    out.u8(u.offer.has_value());
    if (u.offer) {
        const auto& offer = *u.offer;
        if (offer.count < 1 || offer.count > 3) throw std::invalid_argument("invalid offer count");
        out.u64(offer.epoch);
        out.u64(offer.serial);
        out.u8(static_cast<std::uint8_t>(offer.count));
        for (int i = 0; i < offer.count; ++i) out.u8(static_cast<std::uint8_t>(offer.choices[i]));
    }
    out.u8(u.lastChoice.has_value());
    if (u.lastChoice) {
        out.u64(u.lastChoice->epoch);
        out.u64(u.lastChoice->serial);
        out.u8(static_cast<std::uint8_t>(u.lastChoice->id));
    }
    return std::move(out.bytes);
}
std::size_t readSize(Reader& in) {
    const auto v = in.u64();
    if (v > std::numeric_limits<std::size_t>::max()) throw std::invalid_argument("size outside host range");
    return static_cast<std::size_t>(v);
}
}

std::vector<std::uint8_t> encodeSnapshot(const NavigationSnapshot& s) {
    return binary::wrap("T2SN", snapshotFormatVersion, payload(s));
}
std::uint64_t stateHash(const NavigationSnapshot& s) {
    const auto bytes = payload(s);
    return binary::checksum(bytes.data(), bytes.size());
}

NavigationSnapshot decodeSnapshot(const std::vector<std::uint8_t>& bytes) {
    auto in = binary::unwrap(bytes, "T2SN", snapshotFormatVersion, maxSnapshotBytes);
    NavigationSnapshot s;
    auto& m = s.world;
    m.seed = in.u32();
    m.parameters = {readInt(in), readInt(in), in.f32(), readInt(in), readInt(in)};
    const auto count = in.u32();
    if (count != tiny2d::TileMap::checkedCapacity(m.parameters.width, m.parameters.height, m.parameters.tileSize))
        throw std::invalid_argument("tile count differs from dimensions");
    const auto* tiles = in.take(count);
    m.tiles.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (tiles[i] > 1) throw std::invalid_argument("unknown tile type");
        m.tiles.push_back(static_cast<tiny2d::Tile>(tiles[i]));
    }
    m.spawn = readCoord(in);
    m.exit = readCoord(in);
    m.fingerprint = in.u64();
    m.epoch = in.u64();
    m.tick = in.u64();
    m.paused = in.boolean();
    m.movement.position = readPosition(in);
    m.movement.hitX = in.boolean();
    m.movement.hitY = in.boolean();
    m.movement.tilesQueried = readSize(in);
    auto phase = in.u8();
    if (phase > static_cast<unsigned>(NavigationPhase::GameOver)) throw std::invalid_argument("unknown phase");
    s.phase = static_cast<NavigationPhase>(phase);
    s.hp = readInt(in);
    s.vulnerableAt = in.u64();
    s.nextOfferTick = in.u64();
    s.requestedEnemies = readInt(in);
    s.spawnMinimumDistance = readInt(in);
    if (in.boolean()) s.lastDamageSlot = readSize(in);
    s.spawnRng = in.u64();
    const auto enemyCount = in.u32();
    if (enemyCount > 256 || enemyCount > in.remaining()/25)
        throw std::invalid_argument("invalid enemy count");
    s.enemies.resize(enemyCount);
    for (auto& e : s.enemies) {
        e.slot = readSize(in);
        e.epoch = in.u64();
        e.position = readPosition(in);
        if (in.boolean()) e.waypoint = readCoord(in);
    }
    auto& u = s.upgrades;
    for (auto& level : u.levels) level = readInt(in);
    u.rng = in.u64();
    u.serial = in.u64();
    if (in.boolean()) {
        auto& offer = u.offer.emplace();
        offer.epoch = in.u64();
        offer.serial = in.u64();
        offer.count = in.u8();
        if (offer.count < 1 || offer.count > 3) throw std::invalid_argument("invalid offer count");
        for (int i = 0; i < offer.count; ++i) offer.choices[i] = readId(in);
    }
    if (in.boolean()) u.lastChoice = UpgradeChoice{in.u64(), in.u64(), readId(in)};
    in.finish();
    return s;
}
} // namespace tiny2d_game
