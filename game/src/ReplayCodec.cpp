#include "tiny2d_game/Replay.h"
#include "tiny2d_game/SnapshotCodec.h"
#include "BinaryCodec.h"

namespace tiny2d_game {
namespace {

using namespace binary;
constexpr char magic[] = "T2RP";
constexpr std::size_t minEventBytes = 6 * 8 + 1;
constexpr std::size_t maxEventBytes = minEventBytes + 24; // regenerate seed and parameters
static_assert(headerBytes + 4 + maxSnapshotBytes + 4 + maxReplayEvents * maxEventBytes + 8
              <= maxReplayBytes, "every successfully recorded prefix must fit the byte budget");

void writeInput(Writer& out, tiny2d::MapInput input) {
    for (int axis : {input.x, input.y}) {
        if (axis < -1 || axis > 1) throw std::invalid_argument("invalid replay input axis");
        out.u8(static_cast<std::uint8_t>(axis + 1));
    }
}

int readAxis(Reader& in) {
    const auto encoded = in.u8();
    if (encoded > 2) throw std::invalid_argument("invalid replay input axis");
    return int(encoded) - 1;
}

void validateParameters(const tiny2d::MapParameters& p) {
    tiny2d::TileMap::checkedCapacity(p.width, p.height, p.tileSize);
    if (p.width < p.minWidth || p.height < p.minHeight ||
        p.roomCount < 0 || p.roomCount > 64 || p.extraConnections < 0 || p.extraConnections > 256)
        throw std::invalid_argument("invalid regenerate parameters");
}

void writeCommand(Writer& out, const ReplayCommand& command) {
    out.u8(static_cast<std::uint8_t>(command.type()));
    switch (command.type()) {
    case ReplayEventType::AdvanceTick:
        return writeInput(out, std::get<ReplayCommand::Advance>(command.payload).input);
    case ReplayEventType::ManualStep:
        return writeInput(out, std::get<ReplayCommand::Manual>(command.payload).input);
    case ReplayEventType::SetPaused:
        return out.u8(std::get<ReplayCommand::Pause>(command.payload).paused);
    case ReplayEventType::ChooseUpgrade: {
        const auto c = std::get<UpgradeChoice>(command.payload);
        if (static_cast<unsigned>(c.id) >= upgradePool.size()) throw std::invalid_argument("invalid upgrade ID");
        out.u64(c.epoch);
        out.u64(c.serial);
        return out.u8(static_cast<std::uint8_t>(c.id));
    }
    case ReplayEventType::Reset: return;
    case ReplayEventType::Regenerate: {
        const auto& change = std::get<ReplayCommand::Regenerate>(command.payload);
        const auto& p = change.parameters;
        validateParameters(p);
        out.u32(change.seed);
        out.u32(p.width);
        out.u32(p.height);
        out.f32(p.tileSize);
        out.u32(p.roomCount);
        return out.u32(p.extraConnections);
    }
    }
    throw std::invalid_argument("unknown replay tag");
}

int readBounded(Reader& in, unsigned maximum) {
    const auto value = in.u32();
    if (value > maximum) throw std::invalid_argument("regenerate field exceeds limit");
    return static_cast<int>(value);
}

ReplayCommand readCommand(Reader& in) {
    const auto type = static_cast<ReplayEventType>(in.u8());
    switch (type) {
    case ReplayEventType::AdvanceTick: return ReplayCommand::advance({readAxis(in), readAxis(in)});
    case ReplayEventType::ManualStep: return ReplayCommand::manualStep({readAxis(in), readAxis(in)});
    case ReplayEventType::SetPaused: return ReplayCommand::setPaused(in.boolean());
    case ReplayEventType::ChooseUpgrade: {
        const auto epoch = in.u64();
        const auto serial = in.u64();
        const auto id = in.u8();
        if (id >= upgradePool.size()) throw std::invalid_argument("invalid upgrade ID");
        return ReplayCommand::choose({epoch, serial, static_cast<UpgradeId>(id)});
    }
    case ReplayEventType::Reset: return ReplayCommand::reset();
    case ReplayEventType::Regenerate: {
        const auto seed = in.u32();
        tiny2d::MapParameters p;
        p.width = readBounded(in, p.maxWidth);
        p.height = readBounded(in, p.maxHeight);
        p.tileSize = in.f32();
        p.roomCount = readBounded(in, 64);
        p.extraConnections = readBounded(in, 256);
        validateParameters(p);
        return ReplayCommand::regenerate(seed, p);
    }
    }
    throw std::invalid_argument("unknown replay tag");
}

} // namespace

std::vector<std::uint8_t> encodeReplay(const ReplayData& replay) {
    if (replay.events.size() > maxReplayEvents) throw std::length_error("too many replay events");
    const auto base = encodeSnapshot(replay.base);
    Writer out;
    out.bytes.reserve(4 + base.size() + 4 + replay.events.size() * maxEventBytes + 8);
    out.u32(static_cast<std::uint32_t>(base.size()));
    out.bytes.insert(out.bytes.end(), base.begin(), base.end());
    out.u32(static_cast<std::uint32_t>(replay.events.size()));
    for (const auto& event : replay.events) {
        out.u64(event.sequence);
        out.u64(event.beforeEpoch);
        out.u64(event.beforeTick);
        writeCommand(out, event.command);
        out.u64(event.afterEpoch);
        out.u64(event.afterTick);
        out.u64(event.expectedHash);
    }
    out.u64(replay.finalHash);
    return wrap(magic, replayFormatVersion, out.bytes);
}

ReplayData decodeReplay(const std::vector<std::uint8_t>& bytes) {
    auto in = unwrap(bytes, magic, replayFormatVersion, maxReplayBytes);
    const auto baseSize = in.u32();
    if (baseSize > maxSnapshotBytes) throw std::length_error("embedded snapshot exceeds budget");
    const auto* base = in.take(baseSize);
    const auto count = in.u32();
    if (count > maxReplayEvents) throw std::length_error("too many replay events");
    if (in.remaining() < 8 || count > (in.remaining() - 8) / minEventBytes)
        throw std::invalid_argument("insufficient bytes for replay event count");
    ReplayData data;
    data.base = decodeSnapshot(Bytes(base, base + baseSize));
    data.events.reserve(count);
    for (unsigned i = 0; i < count; ++i) {
        ReplayEvent event;
        event.sequence = in.u64();
        event.beforeEpoch = in.u64();
        event.beforeTick = in.u64();
        event.command = readCommand(in);
        event.afterEpoch = in.u64();
        event.afterTick = in.u64();
        event.expectedHash = in.u64();
        data.events.push_back(event);
    }
    data.finalHash = in.u64();
    in.finish();
    return data;
}

} // namespace tiny2d_game
