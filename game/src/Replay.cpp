#include "tiny2d_game/Replay.h"
#include "tiny2d_game/SnapshotCodec.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace tiny2d_game {

const char* eventName(ReplayEventType type) {
    switch (type) {
    case ReplayEventType::AdvanceTick: return "AdvanceTick";
    case ReplayEventType::ManualStep: return "ManualStep";
    case ReplayEventType::SetPaused: return "SetPaused";
    case ReplayEventType::ChooseUpgrade: return "ChooseUpgrade";
    case ReplayEventType::Reset: return "Reset";
    case ReplayEventType::Regenerate: return "Regenerate";
    }
    throw std::invalid_argument("unknown replay event type");
}

bool dispatchEvent(NavigationSession& session, const ReplayCommand& command) {
    const auto& world = session.world();
    switch (command.type()) {
    case ReplayEventType::AdvanceTick:
        if (session.phase() != NavigationPhase::Playing || world.paused()) return false;
        session.fixedUpdate(std::get<ReplayCommand::Advance>(command.payload).input);
        return true;
    case ReplayEventType::ManualStep:
        if (session.phase() != NavigationPhase::Playing || !world.paused()) return false;
        session.step(std::get<ReplayCommand::Manual>(command.payload).input);
        return true;
    case ReplayEventType::SetPaused: {
        const bool paused = std::get<ReplayCommand::Pause>(command.payload).paused;
        if (world.paused() == paused) return false;
        session.setPaused(paused);
        return true;
    }
    case ReplayEventType::ChooseUpgrade: {
        const auto choice = std::get<UpgradeChoice>(command.payload);
        return session.choose(choice.epoch, choice.serial, choice.id);
    }
    case ReplayEventType::Reset:
        session.reset();
        return true;
    case ReplayEventType::Regenerate: {
        const auto& change = std::get<ReplayCommand::Regenerate>(command.payload);
        session.regenerate(change.seed, change.parameters);
        return true;
    }
    }
    throw std::invalid_argument("unknown replay command");
}

ReplayRecorder::ReplayRecorder(const NavigationSession& session)
    : source_(&session), data_{session.captureSnapshot(), {}, 0} {
    data_.finalHash = stateHash(data_.base);
}

bool ReplayRecorder::apply(NavigationSession& session, const ReplayCommand& command) {
    if (&session != source_) throw std::invalid_argument("recorder bound to another live session");
    if (full()) throw std::length_error("replay recording reached 100000 events");
    auto& events = data_.events;
    if (events.size() == events.capacity()) {
        events.reserve(std::min(maxReplayEvents, std::max<std::size_t>(16, 2 * events.size())));
    }
    const auto& world = session.world();
    ReplayEvent event{events.size() + 1, world.epoch(), world.tick(), command, 0, 0, 0};
    // A recording failure (including allocation for hashing) must leave the
    // live world and the recorded prefix at the same event boundary.
    auto candidate = session;
    if (!dispatchEvent(candidate, command)) return false;
    event.afterEpoch = candidate.world().epoch();
    event.afterTick = candidate.world().tick();
    event.expectedHash = stateHash(candidate.captureSnapshot());
    events.push_back(event);
    static_assert(std::is_nothrow_move_assignable_v<NavigationSession>);
    session = std::move(candidate);
    data_.finalHash = event.expectedHash;
    return true;
}

ReplayPlayer::ReplayPlayer(ReplayData data)
    : data_(std::make_shared<const ReplayData>(std::move(data))), session_(NavigationSession::fromSnapshot(data_->base)) {
    if (data_->events.size() > maxReplayEvents) throw std::length_error("too many replay events");
    atBase();
}

ReplayPlayer::ReplayPlayer(std::shared_ptr<const ReplayData> data, NavigationSession session, std::size_t cursor)
    : data_(std::move(data)), session_(std::move(session)), cursor_(cursor) {
    if (cursor_ == data_->events.size()) status_ = PlaybackStatus::Finished;
}

void ReplayPlayer::atBase() {
    cursor_ = 0;
    error_.clear();
    status_ = PlaybackStatus::Paused;
    if (data_->events.empty()) {
        const auto actualHash = stateHash(session_.captureSnapshot());
        if (data_->finalHash == actualHash) {
            status_ = PlaybackStatus::Finished;
        } else {
            std::ostringstream message;
            message << "base snapshot final hash mismatch: expected=0x" << std::hex << data_->finalHash
                    << " actual=0x" << actualHash;
            error_ = message.str();
            status_ = PlaybackStatus::Error;
        }
    }
}

void ReplayPlayer::restart() {
    session_ = NavigationSession::fromSnapshot(data_->base);
    atBase();
}

void ReplayPlayer::setPlaying(bool playing) {
    if (status_ == PlaybackStatus::Playing || status_ == PlaybackStatus::Paused)
        status_ = playing ? PlaybackStatus::Playing : PlaybackStatus::Paused;
}

bool ReplayPlayer::next() {
    if (status_ == PlaybackStatus::Finished || status_ == PlaybackStatus::Error) return false;
    const auto& event = data_->events[cursor_];
    const auto& before = session_.world();
    auto fail = [&](const std::string& reason, const NavigationSession& actual, std::uint64_t actualHash) {
        std::ostringstream message;
        message << "sequence=" << cursor_ + 1 << " stored_sequence=" << event.sequence
                << " event=" << eventName(event.command.type()) << " " << reason
                << " expected_before=" << event.beforeEpoch << "/" << event.beforeTick
                << " actual_before=" << before.epoch() << "/" << before.tick()
                << " expected_after=" << event.afterEpoch << "/" << event.afterTick
                << " actual_state=" << actual.world().epoch() << "/" << actual.world().tick()
                << " expected_hash=0x" << std::hex << event.expectedHash << " actual_hash=0x" << actualHash;
        error_ = message.str();
        status_ = PlaybackStatus::Error;
        return false;
    };

    if (event.sequence != cursor_ + 1 || before.epoch() != event.beforeEpoch ||
        before.tick() != event.beforeTick) {
        return fail("before/sequence mismatch", session_, stateHash(session_.captureSnapshot()));
    }

    // A value copy also copies the bounded navigator cache. No map generation,
    // spawn draw or graph rebuild is required to validate ordinary events.
    auto candidate = session_;
    try {
        if (!dispatchEvent(candidate, event.command))
            return fail("command rejected", candidate, stateHash(candidate.captureSnapshot()));
        const auto hash = stateHash(candidate.captureSnapshot());
        if (candidate.world().epoch() != event.afterEpoch || candidate.world().tick() != event.afterTick ||
            hash != event.expectedHash)
            return fail("after/hash mismatch", candidate, hash);
        const bool last = cursor_ + 1 == data_->events.size();
        if (last && hash != data_->finalHash) {
            std::ostringstream reason;
            reason << "trailer hash mismatch final_hash=0x" << std::hex << data_->finalHash;
            return fail(reason.str(), candidate, hash);
        }
        static_assert(std::is_nothrow_move_assignable_v<NavigationSession>);
        session_ = std::move(candidate);
        ++cursor_;
        if (last) status_ = PlaybackStatus::Finished;
        return true;
    } catch (const std::exception& error) {
        return fail(error.what(), candidate, stateHash(candidate.captureSnapshot()));
    }
}

} // namespace tiny2d_game
