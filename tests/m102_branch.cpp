#include "M5Test.h"
#include "tiny2d_game/ReplayBranch.h"
#include "tiny2d_game/SnapshotCodec.h"

#include <algorithm>
#include <limits>

using namespace tiny2d_game;
namespace {

void identical(const NavigationSnapshot& a, const NavigationSnapshot& b) {
    const auto field = firstDifference(a,b);
    if (!field.empty()) throw std::runtime_error("branch state differs at " + field);
    check(encodeSnapshot(a) == encodeSnapshot(b), "branch canonical bytes differ");
}

ReplayData parentTape() {
    NavigationSession live(71, {}, 0);
    ReplayRecorder recorder(live);
    for (int i=0; i<600; ++i) recorder.apply(live, ReplayCommand::advance());
    const auto offer = *live.upgrades().offer();
    recorder.apply(live, ReplayCommand::choose({offer.epoch, offer.serial, UpgradeId::Vitality}));
    recorder.apply(live, ReplayCommand::setPaused(true));
    recorder.apply(live, ReplayCommand::manualStep({1,0}));
    recorder.apply(live, ReplayCommand::setPaused(false));
    for (int i=0; i<15; ++i) recorder.apply(live, ReplayCommand::advance({1,0}));
    return recorder.data();
}

void forkAtOffer() {
    const auto original = parentTape();
    const auto originalBytes = encodeReplay(original);
    Timeline parent(original);
    parent.seek(600);
    const auto initial = parent.session().captureSnapshot();
    const auto offer = *initial.upgrades.offer;
    rejects([&] { ReplayBranch::fork(parent, 0); });
    rejects([&] { ReplayBranch::fork(parent, parent.recordingId()); });
    check(parent.cursor() == 600, "failed fork preserves parent cursor");
    identical(initial, parent.session().captureSnapshot());

    auto vitality = ReplayBranch::fork(parent, 11);
    auto fleet = ReplayBranch::fork(parent, 12);
    for (const auto* child : {&vitality, &fleet}) {
        const auto meta = child->metadata();
        check(meta.parentId == parent.recordingId() && meta.parentRecordingId == parent.recordingId() &&
              meta.forkCursor == 600 && child->replay().events.empty(), "fork provenance and zero local cursor");
        identical(child->replay().base, initial);
        identical(child->session().captureSnapshot(), initial);
    }
    const auto pick = [&](UpgradeId id) { return ReplayCommand::choose({offer.epoch, offer.serial, id}); };
    check(vitality.apply(pick(UpgradeId::Vitality)) && fleet.apply(pick(UpgradeId::Fleet)), "siblings choose independently");
    check(!fleet.apply(pick(UpgradeId::Vitality)), "same-tick repeated choice rejected");
    check(vitality.session().hp() == 6 && vitality.session().upgrades().playerSpeed() == 120 &&
          fleet.session().hp() == 5 && fleet.session().upgrades().playerSpeed() == 132, "branch attribute divergence");
    for (const auto* child : {&vitality, &fleet}) {
        const auto& e = child->replay().events.at(0);
        check(e.sequence == 1 && e.beforeTick == 600 && e.afterTick == 600 &&
              e.beforeEpoch == offer.epoch && e.afterEpoch == offer.epoch, "choice is first child event at same tick");
    }
    identical(initial, parent.session().captureSnapshot());
    check(parent.cursor() == 600 && encodeReplay(parent.data()) == originalBytes, "all parent future bytes survive forks");

    std::vector<NavigationSnapshot> fleetHistory{fleet.session().captureSnapshot()};
    bool differentPosition = false;
    for (std::size_t i=601; i<original.events.size(); ++i) {
        check(vitality.apply(original.events[i].command) && fleet.apply(original.events[i].command), "suffix accepted");
        fleetHistory.push_back(fleet.session().captureSnapshot());
        differentPosition |= fleet.session().world().playerPosition().x != vitality.session().world().playerPosition().x;
    }
    check(encodeReplay(parent.data()) == originalBytes, "branch suffixes leave immutable parent intact");
    parent.seek(original.events.size());
    identical(vitality.session().captureSnapshot(), parent.session().captureSnapshot());
    check(differentPosition, "Fleet changes production motion");

    ReplayPlayer detached(decodeReplay(encodeReplay(fleet.replay())));
    for (const auto& state : fleetHistory) {
        check(detached.next(), "independent child replay event");
        identical(state, detached.session().captureSnapshot());
    }
    check(detached.status() == PlaybackStatus::Finished, "independent recording reaches tip");
    const auto package = encodeBranch(fleet);
    auto reopened = decodeBranch(package);
    check(encodeBranch(reopened) == package, "standalone branch envelope is canonical");
    identical(fleet.session().captureSnapshot(), reopened.session().captureSnapshot());
    auto moved = std::move(reopened);
    check(moved.apply(ReplayCommand::reset()), "moved imported recorder remains bound to its session");
    check(moved.session().world().tick() == 0 && moved.session().world().epoch() == offer.epoch+1 &&
          moved.replay().events.back().sequence == fleetHistory.size()+1, "append after reopen maintains local sequence");
    check(moved.apply(ReplayCommand::regenerate(123, tiny2d::MapParameters{19,21})), "branch regenerate");
    while (moved.session().world().tick() < 600) moved.apply(ReplayCommand::advance());
    check(moved.session().upgrades().offer() && !moved.apply(pick(UpgradeId::Fleet)), "reject old choice after branch regenerate");
    check(encodeBranch(fleet) == package && encodeReplay(parent.data()) == originalBytes, "reopened edits independent");

    Timeline imported(fleet.replay());
    imported.seek(2);
    const auto parentId = fleet.metadata().branchId;
    check(parentId != imported.recordingId(), "fixture distinguishes logical branch ID from tape digest");
    auto grandchild = ReplayBranch::fork(imported, 13, parentId);
    const auto grand = decodeBranch(encodeBranch(grandchild));
    check(grand.metadata().branchId == 13 && grand.metadata().parentId == parentId &&
          grand.metadata().parentRecordingId == imported.recordingId() && grand.metadata().forkCursor == 2,
          "loaded branch provenance preserves logical and content identity");
    identical(imported.session().captureSnapshot(), grand.session().captureSnapshot());
    rejects([&] { ReplayBranch::fork(imported, parentId, parentId); });
}

void otherBoundaries() {
    Timeline timeline(parentTape());
    for (std::size_t cursor : {0,37,602,603,619}) {
        timeline.seek(cursor);
        auto branch = ReplayBranch::fork(timeline, 101);
        identical(timeline.session().captureSnapshot(), branch.session().captureSnapshot());
        check(branch.metadata().forkCursor == cursor && branch.replay().events.empty(), "new local tape starts empty");
        if (branch.session().world().paused()) {
            const auto bytes = encodeBranch(branch);
            check(!branch.apply(ReplayCommand::advance()), "paused fork rejects ordinary tick");
            check(encodeBranch(branch) == bytes && branch.apply(ReplayCommand::manualStep()), "paused fork accepts manual step");
        } else {
            check(branch.apply(ReplayCommand::advance()), "ordinary fork accepts advance");
        }
        check(timeline.cursor() == cursor && timeline.session().world().tick()+1 == branch.session().world().tick(),
              "parent position remains frozen");
    }
}

void rejectionAndBudget() {
    NavigationSession s(71,tiny2d::MapParameters{5,5},0);
    ReplayRecorder recorder(s);
    Timeline timeline(recorder.data());
    auto branch = ReplayBranch::fork(timeline, 43);
    auto empty = encodeBranch(branch);
    rejects([&] { auto cut=empty; cut.pop_back(); decodeBranch(cut); });
    rejects([&] { auto bad=empty; bad[4]^=1; decodeBranch(bad); });
    rejects([&] { auto bad=empty; bad.back()^=1; decodeBranch(bad); });
    rejects([&] { decodeBranch(std::vector<std::uint8_t>(maxBranchBytes+1)); });

    auto put = [](auto& bytes, std::size_t offset, std::uint64_t value) {
        for (unsigned i=0; i<8; ++i) bytes.at(offset+i)=static_cast<std::uint8_t>(value>>(8*i));
    };
    auto fixChecksum = [&](auto& bytes, std::size_t start) {
        std::uint64_t hash=UINT64_C(14695981039346656037);
        for (std::size_t i=start+24; i<bytes.size(); ++i) hash=(hash^bytes[i])*UINT64_C(1099511628211);
        put(bytes, start+16, hash);
    };
    for (auto offset : {24,48}) {
        auto bad=empty;
        put(bad, offset, offset==24 ? 0 : maxReplayEvents+1);
        fixChecksum(bad, 0);
        rejects([&] { decodeBranch(bad); });
    }
    auto bad=empty;
    bad.back()^=1; // incorrect embedded final hash, clean nested and outer checksums
    fixChecksum(bad, 24+4*8+4);
    fixChecksum(bad, 0);
    rejects([&] { decodeBranch(bad); });
    check(encodeBranch(branch) == empty && timeline.cursor() == 0, "invalid loads preserve original");

    ReplayPlayer unfinished(recorder.data());
    check(unfinished.status()==PlaybackStatus::Finished, "empty recorder verified");
    s.step();
    rejects([&]{ ReplayRecorder wrongTip(s,unfinished); });

    for (std::size_t i=0; i<maxReplayEvents; ++i)
        check(branch.apply(ReplayCommand::setPaused(i%2==0)), "accepted toggle before capacity");
    const auto hash = stateHash(branch.session().captureSnapshot());
    check(branch.full() && branch.replay().events.size()==maxReplayEvents, "exact branch event budget");
    rejects([&]{branch.apply(ReplayCommand::reset());});
    check(branch.full() && hash==stateHash(branch.session().captureSnapshot()) &&
          encodeBranch(branch).size()<=maxBranchBytes, "full prefix preserved and exportable");
}
}

int main() {
    try {
        forkAtOffer();
        otherBoundaries();
        rejectionAndBudget();
        std::cout << "Independent branches: ordinary/pause/offer forks, Vitality vs Fleet, historical and runtime "
                     "ownership, imported branch ancestry, validated independent replay, append and budgets passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
