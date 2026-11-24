#pragma once

#include "tiny2d_game/NavigationSession.h"
#include "tiny2d_game/NavigationSnapshot.h"

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace tiny2d_game {

inline constexpr std::uint32_t replayFormatVersion = 1;
inline constexpr std::size_t maxReplayBytes = 32 * 1024 * 1024;
inline constexpr std::size_t maxReplayEvents = 100000;

enum class ReplayEventType : std::uint8_t {
    AdvanceTick = 0, ManualStep = 1, SetPaused = 2, ChooseUpgrade = 3, Reset = 4, Regenerate = 5
};
const char* eventName(ReplayEventType type);

struct ReplayCommand {
    struct Advance { tiny2d::MapInput input; };
    struct Manual { tiny2d::MapInput input; };
    struct Pause { bool paused; };
    struct Reset {};
    struct Regenerate { std::uint32_t seed; tiny2d::MapParameters parameters; };
    // Variant order is the explicit version-1 event tag.
    std::variant<Advance, Manual, Pause, UpgradeChoice, Reset, Regenerate> payload;
    ReplayEventType type() const { return static_cast<ReplayEventType>(payload.index()); }

    static ReplayCommand advance(tiny2d::MapInput input = {}) { return {Advance{input}}; }
    static ReplayCommand manualStep(tiny2d::MapInput input = {}) { return {Manual{input}}; }
    static ReplayCommand setPaused(bool paused) { return {Pause{paused}}; }
    static ReplayCommand choose(UpgradeChoice choice) { return {choice}; }
    static ReplayCommand reset() { return {Reset{}}; }
    static ReplayCommand regenerate(std::uint32_t seed, const tiny2d::MapParameters& p) {
        return {Regenerate{seed, p}};
    }
};

struct ReplayEvent {
    std::uint64_t sequence, beforeEpoch, beforeTick;
    ReplayCommand command;
    std::uint64_t afterEpoch, afterTick, expectedHash;
};

struct ReplayData {
    NavigationSnapshot base;
    std::vector<ReplayEvent> events;
    std::uint64_t finalHash = 0;
};

// Invalid axes/map parameters throw as in the production session. State-neutral
// commands (including stale choice, blocked tick and repeated pause) return false.
bool dispatchEvent(NavigationSession& session, const ReplayCommand& command);

// While recording, route every authoritative command on this session through apply.
// full() marks a complete, saveable prefix. Further attempts raise length_error.
class ReplayPlayer;

class ReplayRecorder {
public:
    explicit ReplayRecorder(const NavigationSession& session);
    // Resume an independently verified tape on its exact final authority.
    ReplayRecorder(const NavigationSession& session, const ReplayPlayer& finished);
    bool apply(NavigationSession& session, const ReplayCommand& command);
    bool full() const { return data_.events.size() == maxReplayEvents; }
    const ReplayData& data() const { return data_; }

private:
    const NavigationSession* source_;
    ReplayData data_;
};

std::vector<std::uint8_t> encodeReplay(const ReplayData& replay);
ReplayData decodeReplay(const std::vector<std::uint8_t>& bytes);

enum class PlaybackStatus { Playing, Paused, Finished, Error };

class ReplayPlayer {
public:
    explicit ReplayPlayer(ReplayData data);
    const NavigationSession& session() const { return session_; }
    const ReplayData& data() const { return *data_; }
    PlaybackStatus status() const { return status_; }
    std::size_t verifiedCount() const { return cursor_; }
    const std::string& error() const { return error_; }
    // next() verifies exactly one event whether transport is playing or paused.
    // Finished and Error are inert until an explicit restart.
    bool next();
    void setPlaying(bool playing);
    void restart();

private:
    friend class Timeline;
    // Timeline alone resumes from a checkpoint bound to this verified tape.
    ReplayPlayer(std::shared_ptr<const ReplayData> data, NavigationSession session, std::size_t cursor);
    void atBase();
    std::shared_ptr<const ReplayData> data_;
    NavigationSession session_;
    PlaybackStatus status_ = PlaybackStatus::Paused;
    std::size_t cursor_ = 0;
    std::string error_;
};

} // namespace tiny2d_game
