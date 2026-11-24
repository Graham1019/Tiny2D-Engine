// CPU-only file verifier and bounded evidence fixtures for M10.1.
#include "tiny2d_game/Replay.h"
#include "tiny2d_game/SnapshotCodec.h"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>

using namespace tiny2d_game;
using namespace tiny2d;
namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;

namespace {

void require(bool value, const std::string& reason) {
    if (!value) throw std::runtime_error(reason);
}

void summary(const NavigationSession& s) {
    const auto count = std::count_if(s.enemies().begin(), s.enemies().end(),
                                     [](const auto& e) { return e.waypoint.has_value(); });
    std::cout << "epoch=" << s.world().epoch() << " tick=" << s.world().tick()
              << " seed=" << s.world().seed() << " phase=" << phaseName(s.phase())
              << " manual_paused=" << s.world().paused() << " hp=" << s.hp() << '/' << s.upgrades().maxHP()
              << " enemies=" << s.enemies().size() << " waypoints=" << count
              << " offer=" << s.upgrades().offer().has_value()
              << " last_choice=" << (s.upgrades().lastChoice() ? upgradeName(s.upgrades().lastChoice()->id) : "none")
              << " hash=" << std::hex << stateHash(s.captureSnapshot()) << std::dec << '\n';
}

void record(const std::string& scenario, const fs::path& directory, const fs::path& tracePath) {
    require(scenario == "A" || scenario == "B" || scenario == "C", "expected scenario A, B or C");
    NavigationSession live(71, {}, scenario == "B" ? 0 : 16);
    const unsigned initialTicks = scenario == "A" ? 37 : (scenario == "B" ? 600 : 0);
    for (unsigned i = 0; i < initialTicks; ++i) live.fixedUpdate(mapWalkInput(i));
    if (scenario == "A")
        require(std::any_of(live.enemies().begin(), live.enemies().end(),
                            [](const auto& e){ return e.waypoint.has_value(); }), "mid-leg fixture required");
    ReplayRecorder recorder(live);
    writeNewFile(directory/"base.snap", encodeSnapshot(recorder.data().base));
    std::cout << "base ";
    summary(live);
    Bytes trace;
    auto apply = [&](const ReplayCommand& command) {
        require(recorder.apply(live, command), "fixture command rejected");
        auto bytes = encodeSnapshot(live.captureSnapshot());
        const auto length = static_cast<std::uint32_t>(bytes.size());
        for (unsigned i=0; i<4; ++i) trace.push_back(static_cast<std::uint8_t>(length >> (8*i)));
        trace.insert(trace.end(), bytes.begin(), bytes.end());
    };
    auto walk = [&] { apply(ReplayCommand::advance(mapWalkInput(live.world().tick()))); };

    if (scenario == "A") {
        while (live.phase() == NavigationPhase::Playing || live.phase() == NavigationPhase::AwaitingUpgrade) {
            if (const auto& offer = live.upgrades().offer())
                apply(ReplayCommand::choose({offer->epoch, offer->serial, offer->choices[0]}));
            walk();
        }
        require(live.world().tick() == 702 && live.phase() == NavigationPhase::GameOver,
                "unexpected default terminal state");
    } else if (scenario == "B") {
        const auto offer = live.upgrades().offer().value();
        apply(ReplayCommand::setPaused(true));
        apply(ReplayCommand::choose({offer.epoch, offer.serial, UpgradeId::Vitality}));
        require(live.world().tick() == 600 && live.hp() == 6 && live.upgrades().maxHP() == 6 &&
                !live.upgrades().offer() && live.world().paused(), "same-tick choice did not persist");
        writeNewFile(directory/"chosen.snap", encodeSnapshot(live.captureSnapshot()));
        apply(ReplayCommand::manualStep({0,-1}));
        apply(ReplayCommand::setPaused(false));
        while (live.world().tick() < 650) walk();
    } else {
        while (live.world().tick() < 600) walk();
        const auto stale = live.upgrades().offer().value();
        apply(ReplayCommand::reset());
        while (live.world().tick() < 600) walk();
        require(!recorder.apply(live, ReplayCommand::choose({stale.epoch, stale.serial, stale.choices[0]})),
                "old epoch-1 choice accepted at epoch 2");
        const auto old = live.upgrades().offer().value();
        MapParameters p;
        p.width = 45; p.height = 29; p.roomCount = 5; p.extraConnections = 7; p.tileSize = 20;
        apply(ReplayCommand::regenerate(2026, p));
        require(live.world().epoch() == 3 && !live.upgrades().offer() &&
                !recorder.apply(live, ReplayCommand::choose({old.epoch, old.serial, old.choices[0]})),
                "stale regenerate identity");
        for (const auto& e : live.enemies()) require(e.epoch == 3, "stale enemy identity");
        for (int i=0; i<8; ++i) walk();
    }
    writeNewFile(directory/"events.replay", encodeReplay(recorder.data()));
    writeNewFile(directory/"final.snap", encodeSnapshot(live.captureSnapshot()));
    writeNewFile(tracePath, trace);
    std::cout << "recorded=" << recorder.data().events.size() << " final ";
    summary(live);
}

class Trace {
public:
    explicit Trace(const fs::path& path) : bytes_(readBoundedFile(path, maxReplayBytes)) {}
    bool finished() const { return offset_ == bytes_.size(); }
    Bytes next() {
        require(bytes_.size() - offset_ >= 4, "missing canonical trace row");
        std::uint32_t length = 0;
        for (unsigned i = 0; i < 4; ++i) length |= std::uint32_t(bytes_[offset_++]) << (8*i);
        require(length <= maxSnapshotBytes && length <= bytes_.size() - offset_, "invalid canonical trace length");
        auto row = bytes_.begin() + offset_;
        offset_ += length;
        return Bytes(row, row + length);
    }
private:
    Bytes bytes_;
    std::size_t offset_ = 0;
};

void verify(const fs::path& replay, const fs::path* tracePath, const fs::path* basePath,
            const fs::path* finalPath) {
    auto data = decodeReplay(readBoundedFile(replay, maxReplayBytes));
    std::optional<Trace> trace;
    if (tracePath) {
        trace.emplace(*tracePath);
        const auto file = readBoundedFile(*basePath, maxSnapshotBytes);
        require(file == encodeSnapshot(data.base), "standalone and embedded bases differ");
        // Restore the standalone artifact from disk as the actual playback base.
        data.base = NavigationSession::fromSnapshot(decodeSnapshot(file)).captureSnapshot();
        require(file == encodeSnapshot(data.base), "standalone restored base differs");
        std::cout << "sequence,event,before_epoch,before_tick,after_epoch,after_tick,state_hash,canonical_bytes,match\n";
    }

    ReplayPlayer player(std::move(data));
    while (player.next()) {
        if (!trace) continue;
        const auto& event = player.data().events[player.verifiedCount()-1];
        auto expected = trace->next();
        const auto actual = player.session().captureSnapshot();
        const auto diff = firstDifference(decodeSnapshot(expected), actual);
        require(diff.empty() && encodeSnapshot(actual) == expected, "event " + std::to_string(event.sequence) +
                " field/canonical mismatch " + diff);
        std::cout << event.sequence << ',' << eventName(event.command.type()) << ',' << event.beforeEpoch << ','
                  << event.beforeTick << ',' << event.afterEpoch << ',' << event.afterTick << ',' << std::hex
                  << event.expectedHash << std::dec << ',' << expected.size() << ",1\n";
    }
    require(player.status() == PlaybackStatus::Finished, player.error());
    if (trace) {
        require(trace->finished(), "extra canonical trace rows");
        require(readBoundedFile(*finalPath,maxSnapshotBytes) == encodeSnapshot(player.session().captureSnapshot()),
                "final canonical snapshot differs");
    } else {
        std::cout << "verified=" << player.verifiedCount() << " final ";
        summary(player.session());
    }
}

void reject(const std::string& kind, const fs::path& file) {
    NavigationSession old(2026);
    old.step({-1,0});
    const auto before = encodeSnapshot(old.captureSnapshot());
    std::string error;
    if (kind == "snapshot") {
        try { old.restoreSnapshot(decodeSnapshot(readBoundedFile(file, maxSnapshotBytes))); }
        catch (const std::exception& e) { error = e.what(); }
    } else {
        require(kind == "replay", "invalid rejection kind");
        ReplayPlayer player(decodeReplay(readBoundedFile(file, maxReplayBytes)));
        Bytes lastGood = encodeSnapshot(player.session().captureSnapshot());
        while (player.next()) lastGood = encodeSnapshot(player.session().captureSnapshot());
        require(player.status() == PlaybackStatus::Error && player.verifiedCount() == 2 &&
                !player.next() && encodeSnapshot(player.session().captureSnapshot()) == lastGood,
                "incorrect first failure or changed last verified state");
        error = player.error();
    }
    require(!error.empty() && encodeSnapshot(old.captureSnapshot()) == before, "load replaced old live state");
    std::cout << "rejected=" << file.filename().string() << " live_bytes_preserved=1 " << error << '\n';
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::string action = argc > 1 ? argv[1] : "";
        if (action == "record" && argc == 5) record(argv[2],argv[3],argv[4]);
        else if (action == "verify" && argc == 3) verify(argv[2],nullptr,nullptr,nullptr);
        else if (action == "verify-trace" && argc == 6) {
            const fs::path trace = argv[3], base = argv[4], final = argv[5];
            verify(argv[2],&trace,&base,&final);
        } else if (action == "snapshot" && argc == 3)
            summary(NavigationSession::fromSnapshot(decodeSnapshot(readBoundedFile(argv[2],maxSnapshotBytes))));
        else if (action == "reject" && argc == 4) reject(argv[2],argv[3]);
        else throw std::invalid_argument("record A|B|C DIRECTORY TRACE | verify REPLAY | verify-trace REPLAY TRACE BASE FINAL | snapshot FILE | reject snapshot|replay FILE");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
