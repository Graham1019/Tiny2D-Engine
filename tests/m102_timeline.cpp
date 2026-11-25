#include "M5Test.h"
#include "tiny2d_game/Timeline.h"
#include "tiny2d_game/SnapshotCodec.h"

#include <algorithm>
#include <numeric>
#include <random>

using namespace tiny2d_game;
using namespace tiny2d;
using Bytes = std::vector<std::uint8_t>;
using Command = ReplayCommand;

namespace tiny2d_game {
struct TimelineTestAccess {
    static TimelineCheckpoint& at(Timeline& timeline, std::size_t index) {
        return timeline.checkpoints_.at(index);
    }
};
}

namespace {
struct Fixture {
    ReplayData tape;
    std::vector<Bytes> expected;
};

Fixture fixture(bool mortal) {
    MapParameters p;
    if (!mortal) { p.width = 11; p.height = 9; }
    NavigationSession live(71, p, mortal ? 16 : 0);
    ReplayRecorder recorder(live);
    Fixture result;
    auto remember = [&] { result.expected.push_back(encodeSnapshot(live.captureSnapshot())); };
    remember();
    auto apply = [&](const ReplayCommand& cmd) {
        check(recorder.apply(live, cmd), "fixture command was rejected");
        remember();
    };
    while (live.phase() == NavigationPhase::Playing || live.phase() == NavigationPhase::AwaitingUpgrade) {
        if (const auto offer = live.upgrades().offer()) {
            apply(Command::setPaused(true));
            apply(Command::choose({offer->epoch, offer->serial, offer->choices[0]}));
            // Resume and pause at exactly the same epoch/tick as the first pause.
            apply(Command::setPaused(false));
            apply(Command::setPaused(true));
            apply(Command::manualStep(mapWalkInput(live.world().tick())));
            apply(Command::setPaused(false));
        } else apply(Command::advance(mapWalkInput(live.world().tick())));
    }
    if (mortal) check(live.phase() == NavigationPhase::GameOver && live.world().tick() == 702, "fixture game over");
    else check(live.phase() == NavigationPhase::Victory && live.world().tick() == 3600, "fixture victory");
    apply(Command::reset());
    apply(Command::advance({1,1}));
    p.width=19; p.height=15;
    apply(Command::regenerate(90,p));
    apply(Command::advance({-1,0}));
    result.tape = recorder.data();
    return result;
}

void checkCursor(const Timeline& timeline, const Fixture& f, std::size_t cursor) {
    check(timeline.cursor() == cursor, "incorrect applied-event count");
    const auto state = timeline.session().captureSnapshot();
    const auto difference = firstDifference(decodeSnapshot(f.expected.at(cursor)), state);
    if (!difference.empty()) throw std::runtime_error("cursor " + std::to_string(cursor) + " field " + difference);
    check(encodeSnapshot(state) == f.expected[cursor], "canonical bytes differ");
}

void navigate(const Fixture& f) {
    Timeline timeline(f.tape, {7});
    check(timeline.worstReplay() <= 6 && timeline.maxGap() <= 7, "checkpoint bound");
    checkCursor(timeline,f,0);
    for (std::size_t k=1; k<f.expected.size(); ++k) {
        check(timeline.next(), "expected next");
        checkCursor(timeline,f,k);
    }
    check(!timeline.next(), "end cursor inert");
    std::vector<std::size_t> order(f.expected.size());
    std::iota(order.begin(),order.end(),0);
    // Compare both forward sequential seeks and a reproducibly shuffled visit
    // to every cursor, including terminal, same-tick and new-epoch boundaries.
    for (int pass=0; pass<2; ++pass) {
        for (auto k : order) {
            auto result = timeline.seek(k);
            check(result.checkpointCursor == (k/7)*7 && result.replayedEvents == k%7,
                  "seek selects nearest earlier checkpoint");
            check(result.restoreUs >= 0 && result.replayUs >= 0 && result.totalUs >= result.restoreUs,
                  "seek timing reports");
            checkCursor(timeline,f,k);
        }
        std::mt19937 random(71);
        std::shuffle(order.begin(), order.end(), random);
    }
    auto position = timeline.cursor();
    rejects([&] { timeline.seek(f.expected.size()); });
    checkCursor(timeline, f, position);

    auto bad = f.tape;
    bad.events[12].expectedHash ^= 1;
    rejects([&]{ Timeline invalid(bad); });
    auto changed = f.tape;
    // A shorter valid tape with the same base and prefix has its own identity.
    changed.events.pop_back();
    changed.finalHash = changed.events.back().expectedHash;
    Timeline shortTape(std::move(changed));
    check(shortTape.recordingId() != timeline.recordingId(), "identity covers complete history");
}

void damaged(const Fixture& f) {
    Timeline timeline(f.tape, {5});
    timeline.seek(3);
    auto& cp = TimelineTestAccess::at(timeline, 2);
    const auto intact = cp;
    auto fail = [&] {
        rejects([&] { timeline.seek(intact.cursor+1); });
        checkCursor(timeline,f,3);
        check(timeline.lastSeek().replayedEvents == 3, "failed seek did not publish metrics");
        cp = intact;
    };
    cp.recordingId ^= 1;
    fail();
    cp.expectedHash ^= 1;
    fail();
    cp.snapshotBytes.back() ^= 1;
    fail();
    cp.snapshotBytes = f.expected[intact.cursor+1]; // valid structural and semantic snapshot at wrong cursor
    fail();
    check(timeline.next(), "previous view can still play");
    checkCursor(timeline,f,4);
}

void stress() {
    MapParameters p;
    p.width = p.height = 5;
    NavigationSession live(0,p,0);
    ReplayRecorder recorder(live);
    while (!recorder.full()) recorder.apply(live, Command::setPaused(!live.world().paused()));
    const auto& tape = recorder.data();
    const TimelineOptions limits{2, 4096, 8};
    Timeline timeline(tape, limits);
    Timeline again(tape, limits);
    check(timeline.checkpoints().size() <= limits.maxCheckpoints &&
          timeline.checkpointBytes() <= limits.byteBudget, "sparse index retention limits");
    check(timeline.checkpoints().size() == again.checkpoints().size(), "deterministic retention count");
    for (std::size_t i = 0; i < timeline.checkpoints().size(); ++i) {
        const auto& cp = timeline.checkpoints()[i];
        check(cp.cursor == again.checkpoints()[i].cursor && cp.recordingId == timeline.recordingId(),
              "deterministic retention cursors and identities");
    }
    check(timeline.checkpoints().front().cursor == 0 && timeline.maxGap() > limits.interval,
          "pinned base and deterministic thinning exercised");
    std::size_t observedWorst = 0;
    for (std::size_t i = 1; i < timeline.checkpoints().size(); ++i) {
        const auto k = timeline.checkpoints()[i].cursor-1;
        observedWorst = std::max(observedWorst, timeline.seek(k).replayedEvents);
    }
    observedWorst = std::max(observedWorst,timeline.seek(maxReplayEvents).replayedEvents);
    check(observedWorst == timeline.worstReplay(), "actual worst replay distance");
    for (std::size_t k : {0, 1, 12345, 65000, 99999, 100000}) {
        timeline.seek(k);
        auto expected = tape.base;
        expected.world.paused = k%2 != 0;
        check(encodeSnapshot(expected) == encodeSnapshot(timeline.session().captureSnapshot()),
              "full 100000-event trace remains accessible");
    }
    Timeline pinned(tape, {0});
    check(pinned.checkpoints().size() == 1 && pinned.worstReplay() == maxReplayEvents, "explicit base-only");
    check(pinned.seek(123).replayedEvents == 123 && pinned.session().world().paused(), "base fallback");
    Timeline tiny(tape, {1, pinned.checkpointBytes(), 8});
    check(tiny.checkpoints().size() == 1 && tiny.checkpointBytes() <= pinned.checkpointBytes(),
          "byte-cap thinning converges to base");
    rejects([&] { Timeline tooSmall(tape, {1, pinned.checkpointBytes()-1, 8}); });
    rejects([&] { Timeline noBase(tape, {1, maxTimelineBytes, 0}); });
    rejects([&] { Timeline tooMany(tape, {1, maxTimelineBytes, maxTimelineCheckpoints+1}); });
    rejects([&] { Timeline tooLarge(tape, {1, maxTimelineBytes+1, 8}); });
    NavigationSession emptyLive;
    Timeline empty(ReplayRecorder(emptyLive).data());
    check(empty.maxGap() == 0 && empty.worstReplay() == 0 && !empty.next() &&
          empty.seek(0).replayedEvents == 0, "empty tape has one legal cursor");
    std::cout << "stress events=" << tape.events.size() << " checkpoints=" << timeline.checkpoints().size()
              << " retained_bytes=" << timeline.checkpointBytes() << " worst_replay=" << observedWorst << '\n';
}
}

int main() {
    try {
        const auto mortality = fixture(true);
        const auto victory = fixture(false);
        navigate(mortality);
        navigate(victory);
        damaged(mortality);
        stress();
        std::cout << "all-cursor sequential and random verification passed: "
                  << mortality.expected.size() + victory.expected.size() << " positions\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
