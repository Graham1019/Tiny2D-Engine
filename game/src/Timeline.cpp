#include "tiny2d_game/Timeline.h"
#include "tiny2d_game/SnapshotCodec.h"
#include "BinaryCodec.h"

#include <chrono>
#include <type_traits>

namespace tiny2d_game {
namespace {
using Clock = std::chrono::steady_clock;
using Microseconds = std::chrono::duration<double, std::micro>;

std::uint64_t identity(const ReplayData& tape) {
    const auto bytes = encodeReplay(tape);
    return binary::checksum(bytes.data(), bytes.size());
}
}

Timeline::Timeline(ReplayData data, TimelineOptions options)
    : view_(std::move(data)), options_(options), stride_(options.interval),
      recordingId_(identity(view_.data())), baseHash_(stateHash(view_.data().base)) {
    if (options_.maxCheckpoints < 1 || options_.maxCheckpoints > maxTimelineCheckpoints ||
        options_.byteBudget > maxTimelineBytes)
        throw std::invalid_argument("timeline options exceed checkpoint limits");
    auto base = encodeSnapshot(view_.data().base);
    if (sizeof(TimelineCheckpoint) + base.capacity() > options_.byteBudget)
        throw std::length_error("checkpoint budget cannot hold the pinned base");
    checkpoints_.reserve(1);
    payloadBytes_ = base.capacity();
    checkpoints_.push_back({0, baseHash_, recordingId_, std::move(base)});
    auto verifier = view_; // ReplayPlayer copies share their immutable tape.
    while (verifier.next()) {
        if (stride_ && verifier.verifiedCount() % stride_ == 0 && options_.maxCheckpoints > 1)
            retain(verifier);
    }
    if (verifier.status() == PlaybackStatus::Error) throw std::invalid_argument(verifier.error());
}

std::uint64_t Timeline::expectedAt(std::size_t cursor) const {
    return cursor ? data().events.at(cursor-1).expectedHash : baseHash_;
}

std::size_t Timeline::checkpointBytes() const {
    return payloadBytes_ + checkpoints_.capacity() * sizeof(TimelineCheckpoint);
}

void Timeline::retain(const ReplayPlayer& verifier) {
    const auto cursor = verifier.verifiedCount();
    auto bytes = encodeSnapshot(verifier.session().captureSnapshot());
    while (true) {
        auto capacity = checkpoints_.capacity();
        if (checkpoints_.size() == capacity)
            capacity = std::min(options_.maxCheckpoints, capacity * 2);
        // Account for the entire metadata allocation, even its unused slots.
        const auto total = capacity * sizeof(TimelineCheckpoint) + payloadBytes_ + bytes.capacity();
        if (checkpoints_.size() < options_.maxCheckpoints && total <= options_.byteBudget) {
            checkpoints_.reserve(capacity);
            payloadBytes_ += bytes.capacity();
            checkpoints_.push_back({cursor, expectedAt(cursor), recordingId_, std::move(bytes)});
            return;
        }
        // Only called on an aligned cursor <= maxReplayEvents. Doubling then
        // cannot overflow, and an interval larger than the tape selects only 0.
        stride_ *= 2;
        const auto firstUnused = std::remove_if(checkpoints_.begin(), checkpoints_.end(), [&](const auto& cp) {
            if (cp.cursor % stride_ == 0) return false;
            payloadBytes_ -= cp.snapshotBytes.capacity();
            return true;
        });
        checkpoints_.erase(firstUnused, checkpoints_.end());
        if (cursor % stride_) return;
    }
}

std::size_t Timeline::maxGap() const {
    auto maximum = data().events.size() - checkpoints_.back().cursor;
    for (std::size_t i=1; i<checkpoints_.size(); ++i)
        maximum = std::max(maximum, checkpoints_[i].cursor - checkpoints_[i-1].cursor);
    return maximum;
}

std::size_t Timeline::worstReplay() const {
    auto maximum = data().events.size() - checkpoints_.back().cursor;
    for (std::size_t i=1; i<checkpoints_.size(); ++i)
        maximum = std::max(maximum, checkpoints_[i].cursor - checkpoints_[i-1].cursor - 1);
    return maximum;
}

TimelineSeek Timeline::seek(std::size_t target) {
    const auto start = Clock::now();
    if (target > data().events.size()) throw std::out_of_range("timeline cursor exceeds event count");
    const auto after = std::upper_bound(checkpoints_.begin(), checkpoints_.end(), target,
        [](std::size_t position, const TimelineCheckpoint& cp) { return position < cp.cursor; });
    if (after == checkpoints_.begin()) throw std::invalid_argument("timeline is missing its base checkpoint");
    const auto& cp = *std::prev(after);
    if (cp.recordingId != recordingId_ || cp.expectedHash != expectedAt(cp.cursor))
        throw std::invalid_argument("checkpoint recording or expected hash mismatch at cursor " + std::to_string(cp.cursor));
    auto state = [&] {
        try {
            auto candidate = NavigationSession::fromSnapshot(decodeSnapshot(cp.snapshotBytes));
            const auto restored = candidate.captureSnapshot();
            const auto& tape = data();
            const auto epoch = cp.cursor ? tape.events[cp.cursor-1].afterEpoch : tape.base.world.epoch;
            const auto tick = cp.cursor ? tape.events[cp.cursor-1].afterTick : tape.base.world.tick;
            if (restored.world.epoch != epoch || restored.world.tick != tick ||
                stateHash(restored) != cp.expectedHash || encodeSnapshot(restored) != cp.snapshotBytes)
                throw std::invalid_argument("restored checkpoint content mismatch");
            return candidate;
        } catch (const std::exception& error) {
            throw std::invalid_argument("checkpoint cursor " + std::to_string(cp.cursor) + ": " + error.what());
        }
    }();
    ReplayPlayer candidate(view_.data_, std::move(state), cp.cursor);
    const auto resumed = Clock::now();
    while (candidate.verifiedCount() < target) {
        if (!candidate.next()) throw std::runtime_error(candidate.error());
    }
    const auto verified = Clock::now();
    static_assert(std::is_nothrow_move_assignable_v<ReplayPlayer>);
    view_ = std::move(candidate);
    lastSeek_ = {cp.cursor, target-cp.cursor, Microseconds(resumed-start).count(),
                 Microseconds(verified-resumed).count(), Microseconds(Clock::now()-start).count()};
    return lastSeek_;
}

bool Timeline::next() {
    if (view_.next()) return true;
    if (view_.status() == PlaybackStatus::Error) throw std::runtime_error(view_.error());
    return false;
}

} // namespace tiny2d_game
