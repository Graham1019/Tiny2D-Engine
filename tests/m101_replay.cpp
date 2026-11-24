#include "M5Test.h"
#include "tiny2d/FixedTimestep.h"
#include "tiny2d_game/Replay.h"
#include "tiny2d_game/SnapshotCodec.h"

#include <algorithm>
#include <cstring>

using namespace tiny2d_game;
using namespace tiny2d;
using Command = ReplayCommand;

namespace {
void equal(const NavigationSnapshot& a, const NavigationSnapshot& b) {
    const auto diff = firstDifference(a, b);
    if (!diff.empty()) throw std::runtime_error("snapshot difference at " + diff);
    check(encodeSnapshot(a) == encodeSnapshot(b), "complete canonical bytes differ");
}

void dispatch() {
    NavigationSession live(71, {}, 0);
    for (int i = 0; i < 599; ++i) live.step();
    auto direct = live;
    ReplayRecorder recorder(live);
    auto apply = [&](Command command, bool accepted) {
        const auto before = recorder.data().events.size();
        check(recorder.apply(live, command) == accepted, "dispatch acceptance");
        check(recorder.data().events.size() == before + accepted, "rejected event retained");
        equal(live.captureSnapshot(), direct.captureSnapshot());
    };
    apply(Command::manualStep(), false);
    apply(Command::setPaused(false), false);
    rejects([&] { recorder.apply(live, Command::advance({2,0})); });
    rejects([&] { recorder.apply(direct, Command::advance()); });
    direct.fixedUpdate({-1, 1});
    apply(Command::advance({-1, 1}), true);
    check(live.phase() == NavigationPhase::AwaitingUpgrade, "tick 600 offer");
    apply(Command::advance(), false);
    direct.setPaused(true);
    apply(Command::setPaused(true), true);
    apply(Command::manualStep(), false);
    const auto offer = *live.upgrades().offer();
    const UpgradeChoice choice{offer.epoch, offer.serial, UpgradeId::Vitality};
    check(direct.choose(choice.epoch, choice.serial, choice.id), "accepted Vitality");
    apply(Command::choose(choice), true);
    apply(Command::choose(choice), false);
    check(live.world().tick() == 600 && live.world().paused() && live.hp() == 6,
          "same tick choose while manually paused");
    apply(Command::advance(), false);
    direct.step({0,1});
    apply(Command::manualStep({0,1}), true);
    direct.setPaused(false);
    apply(Command::setPaused(false), true);
    direct.reset();
    apply(Command::reset(), true);
    const auto reset = recorder.data().events.back();
    check(reset.sequence == 6 && reset.beforeTick == 601 && reset.afterTick == 0 &&
          reset.afterEpoch == reset.beforeEpoch + 1, "global event order survives reset");
    MapParameters changed;
    changed.width = 43; changed.height = 31; changed.roomCount = 3;
    changed.extraConnections = 7; changed.tileSize = 17.5f;
    direct.regenerate(1234, changed);
    apply(Command::regenerate(1234, changed), true);
    apply(Command::choose(choice), false);
    changed.width = 0;
    rejects([&] { recorder.apply(live, Command::regenerate(0, changed)); });
    equal(live.captureSnapshot(), direct.captureSnapshot());

    ReplayPlayer player(decodeReplay(encodeReplay(recorder.data())));
    while (player.next()) {}
    check(player.status() == PlaybackStatus::Finished && player.verifiedCount() == 7, "control sequence verified");
    equal(player.session().captureSnapshot(), live.captureSnapshot());
}

struct Run {
    ReplayData replay;
    std::vector<NavigationSnapshot> history;
};

Run run(std::uint32_t seed, int enemies, MapParameters p, int hz,
        unsigned startTick, unsigned ticksToRun) {
    NavigationSession live(seed, p, enemies);
    for (unsigned i = 0; i < startTick; ++i) live.step(mapWalkInput(i));
    ReplayRecorder recorder(live);
    FixedTimestep clock;
    Run result;
    auto apply = [&](const Command& cmd) {
        check(recorder.apply(live, cmd), "accepted live event");
        result.history.push_back(live.captureSnapshot());
    };
    while (live.world().tick() < startTick + ticksToRun) {
        const auto due = clock.advance(1. / hz);
        for (unsigned i = 0; i < due && live.world().tick() < startTick + ticksToRun; ++i) {
            if (const auto& offer = live.upgrades().offer())
                apply(Command::choose({offer->epoch, offer->serial, offer->choices[0]}));
            if (live.phase() != NavigationPhase::Playing) break;
            apply(Command::advance(mapWalkInput(live.world().tick())));
        }
        if (live.phase() == NavigationPhase::Victory || live.phase() == NavigationPhase::GameOver) {
            check(!recorder.apply(live, Command::advance()), "terminal state cannot advance");
            break;
        }
    }
    result.replay = recorder.data();
    return result;
}

void verify(const Run& run) {
    auto bytes = encodeReplay(run.replay);
    auto decoded = decodeReplay(bytes);
    check(bytes == encodeReplay(decoded), "canonical replay roundtrip");
    ReplayPlayer player(std::move(decoded));
    for (const auto& state : run.history) {
        player.setPlaying(true);
        player.setPlaying(false);
        check(player.status() == PlaybackStatus::Paused, "transport pause separated");
        check(player.next(), "event verification");
        equal(player.session().captureSnapshot(), state);
    }
    player.setPlaying(true);
    check(player.status() == PlaybackStatus::Finished && !player.next(), "finished is inert");
    player.restart();
    equal(player.session().captureSnapshot(), run.replay.base);
    check(player.verifiedCount() == 0, "restart position");
}

void mutations(const Run& run) {
    auto fail = [&](auto mutate, std::size_t index) {
        auto changed = run.replay;
        mutate(changed);
        ReplayPlayer player(decodeReplay(encodeReplay(changed)));
        for (std::size_t i = 0; i < index; ++i) check(player.next(), "valid prefix");
        const auto before = player.session().captureSnapshot();
        check(!player.next() && player.status() == PlaybackStatus::Error, "expected mismatch");
        check(player.error().find("sequence=" + std::to_string(index + 1) + " ") != std::string::npos &&
              player.error().find("actual_hash=") != std::string::npos, "first difference details");
        player.setPlaying(true);
        check(!player.next() && player.verifiedCount() == index, "rejected suffix stays stopped");
        equal(before, player.session().captureSnapshot());
    };
    fail([](auto& r) { r.events.insert(r.events.begin()+2, r.events[0]); }, 2);
    fail([](auto& r) { r.events.erase(r.events.begin()+2); }, 2);
    fail([](auto& r) { std::swap(r.events[2], r.events[3]); }, 2);
    fail([](auto& r) { r.events[2].command = Command::manualStep(); }, 2);
    fail([](auto& r) { r.events[2].command = Command::setPaused(true); }, 2);
    fail([](auto& r) { ++r.events[2].beforeEpoch; }, 2);
    fail([](auto& r) { ++r.events[2].beforeTick; }, 2);
    fail([](auto& r) { ++r.events[2].afterEpoch; }, 2);
    fail([](auto& r) { ++r.events[2].afterTick; }, 2);
    fail([](auto& r) { ++r.events[2].expectedHash; }, 2);
    fail([](auto& r) { ++r.finalHash; }, run.history.size()-1);
}

void replay() {
    const auto reference = run(71, 16, {}, 60, 43, 3600);
    check(reference.replay.base.world.tick == 43 && reference.history.back().world.tick == 702 &&
          reference.history.back().phase == NavigationPhase::GameOver, "nonzero base to actual game over");
    verify(reference);
    for (int hz : {30, 120}) {
        auto other = run(71, 16, {}, hz, 43, 3600);
        check(encodeReplay(reference.replay) == encodeReplay(other.replay), "frame-independent recorded data");
        verify(other);
    }
    auto noEnemies = run(73, 0, {}, 60, 0, 3600);
    check(noEnemies.history.back().phase == NavigationPhase::Victory, "recorded victory");
    verify(noEnemies);
    MapParameters maximum;
    maximum.width = 128;
    maximum.height = 96;
    const auto stress = run(2026, 256, maximum, 120, 5, 8);
    verify(stress);
    mutations(stress);

    NavigationSession live;
    auto empty = ReplayRecorder(live).data();
    ReplayPlayer finished(empty);
    check(finished.status() == PlaybackStatus::Finished && !finished.next(), "empty verified recording");
    ++empty.finalHash;
    ReplayPlayer wrong(empty);
    check(wrong.status() == PlaybackStatus::Error && !wrong.next(), "invalid empty trailer");
}

using Bytes = std::vector<std::uint8_t>;
void put(Bytes& bytes, std::size_t offset, std::uint64_t value, int length) {
    for (int i=0; i<length; ++i) bytes.at(offset+i) = static_cast<std::uint8_t>(value >> (8*i));
}
void repair(Bytes& bytes) {
    put(bytes, 12, bytes.size()-24, 4);
    std::uint64_t h = UINT64_C(14695981039346656037);
    for (std::size_t i=24; i<bytes.size(); ++i) h = (h ^ bytes[i]) * UINT64_C(1099511628211);
    put(bytes, 16, h, 8);
}

void codec() {
    MapParameters small;
    small.width = small.height = 5;
    NavigationSession live(0, small, 0);
    ReplayRecorder recorder(live);
    recorder.apply(live, Command::setPaused(true));
    recorder.apply(live, Command::manualStep({-1,1}));
    const auto valid = encodeReplay(recorder.data());
    const auto count = 28 + encodeSnapshot(recorder.data().base).size();
    const auto tag = count + 4 + 24;
    auto invalid = [&](auto mutate, bool checksum=false) {
        auto bytes = valid;
        mutate(bytes);
        if (checksum) repair(bytes);
        rejects([&] { decodeReplay(bytes); });
    };
    for (std::size_t i : {0, 4, 8, 12, 16}) invalid([i](auto& b){ ++b[i]; });
    invalid([](auto& b){ b.resize(12); });
    invalid([](auto& b){ b.pop_back(); }, true);
    invalid([](auto& b){ b.push_back(0); }, true);
    invalid([](auto& b){ put(b, 24, UINT32_MAX, 4); }, true);
    invalid([&](auto& b){ put(b, count, maxReplayEvents+1, 4); }, true);
    invalid([&](auto& b){ put(b, count, 200, 4); }, true);
    invalid([&](auto& b){ b[tag]=253; }, true);
    invalid([&](auto& b){ b[tag+1]=2; }, true);
    invalid([&](auto& b){ b[tag+51]=9; }, true); // invalid x in second command
    invalid([](auto& b){ b.resize(maxReplayBytes+1); });

    while (!recorder.full()) {
        recorder.apply(live, Command::setPaused(!live.world().paused()));
    }
    const auto atLimit = live.captureSnapshot();
    rejects([&] { recorder.apply(live, Command::reset()); });
    equal(atLimit, live.captureSnapshot());
    auto data = decodeReplay(encodeReplay(recorder.data()));
    check(data.events.size() == maxReplayEvents, "100000-event prefix survives");
    data.events.push_back(data.events.back());
    rejects([&] { encodeReplay(data); });
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::invalid_argument("expected dispatch, replay or codec");
        const std::string mode = argv[1];
        if (mode == "dispatch") dispatch();
        else if (mode == "replay") replay();
        else if (mode == "codec") codec();
        else throw std::invalid_argument("unknown replay test suite");
        std::cout << "M10.1 " << mode << " passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
