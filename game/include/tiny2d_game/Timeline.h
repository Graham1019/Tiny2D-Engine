#pragma once

#include "tiny2d_game/Replay.h"

namespace tiny2d_game {

inline constexpr std::size_t maxTimelineBytes = 64 * 1024 * 1024;
inline constexpr std::size_t maxTimelineCheckpoints = 4096;

struct TimelineOptions {
    // Zero selects base-only verification. Budgets include the pinned base.
    std::size_t interval = 120;
    std::size_t byteBudget = maxTimelineBytes;
    std::size_t maxCheckpoints = maxTimelineCheckpoints;
};

struct TimelineCheckpoint {
    std::size_t cursor;
    std::uint64_t expectedHash, recordingId;
    std::vector<std::uint8_t> snapshotBytes;
};

struct TimelineSeek {
    std::size_t checkpointCursor = 0, replayedEvents = 0;
    double restoreUs = 0, replayUs = 0, totalUs = 0;
};

// Event count is the position. Cursor zero is the base; cursor k follows
// events[k-1]. Immutable source events already provide a direct position index.
// The constructor verifies every event and publishes a view positioned at zero.
class Timeline {
public:
    explicit Timeline(ReplayData data, TimelineOptions options = {});
    const NavigationSession& session() const { return view_.session(); }
    const ReplayData& data() const { return view_.data(); }
    std::size_t cursor() const { return view_.verifiedCount(); }
    std::uint64_t recordingId() const { return recordingId_; }
    const std::vector<TimelineCheckpoint>& checkpoints() const { return checkpoints_; }
    // Actual retained capacities of checkpoint metadata and canonical byte buffers.
    // The shared immutable recording and transient restore candidates are separate.
    std::size_t checkpointBytes() const;
    std::size_t maxGap() const;
    std::size_t worstReplay() const;
    const TimelineSeek& lastSeek() const { return lastSeek_; }
    TimelineSeek seek(std::size_t target); // Throws without changing the current view.
    bool next(); // One verified event from the displayed state; false at the end.

private:
    friend struct TimelineTestAccess;
    void retain(const ReplayPlayer& verifier);
    std::uint64_t expectedAt(std::size_t cursor) const;
    ReplayPlayer view_;
    TimelineOptions options_;
    std::size_t stride_;
    std::uint64_t recordingId_, baseHash_;
    std::vector<TimelineCheckpoint> checkpoints_;
    std::size_t payloadBytes_ = 0;
    TimelineSeek lastSeek_;
};

} // namespace tiny2d_game
