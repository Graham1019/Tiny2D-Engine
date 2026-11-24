#include "ReplayApp.h"
#include "tiny2d_game/SnapshotCodec.h"
#include <imgui.h>
#include <iomanip>
#include <iostream>

namespace lab {
using namespace tiny2d_game;

ReplayApp::ReplayApp(const Options& options,NavigationSession& live) {
    if (!options.snapshotIn.empty()) loadSnapshot(options.snapshotIn,live);
    if (!options.replayPath.empty()) {
        loadReplay(options.replayPath);
        player_->setPlaying(true);
    }
    if (!options.recordPath.empty()) {
        startRecording(live);
        launchRecording_=recorder_;
    }
}

const NavigationSession& ReplayApp::displayed(const NavigationSession& live) const {
    return watching_ ? player_->session() : live;
}

const ReplayData* ReplayApp::tape() const {
    if (recorder_) return &recorder_->data();
    return player_ ? &player_->data() : nullptr;
}

const char* ReplayApp::mode() const {
    if (!watching_) return recording_ ? "Recording" : "Live";
    switch (player_->status()) {
    case PlaybackStatus::Paused: return "Playback paused";
    case PlaybackStatus::Playing: return "Playback playing";
    case PlaybackStatus::Finished: return "Playback finished";
    case PlaybackStatus::Error: return "Playback error";
    }
    throw std::logic_error("unknown playback status");
}

void ReplayApp::startRecording(const NavigationSession& live) {
    auto candidate=std::make_shared<ReplayRecorder>(live);
    recorder_=std::move(candidate);
    player_.reset();
    watching_=false;
    recording_=true;
    diagnostic_.clear();
}

bool ReplayApp::dispatch(NavigationSession& live,const ReplayCommand& command) {
    if (watching_) return false;
    if (!recording_) return dispatchEvent(live,command);
    const bool applied=recorder_->apply(live,command);
    if (recorder_->full()) {
        recording_=false;
        diagnostic_="Recording budget reached. The recorded events are available to save.";
        std::cerr<<diagnostic_<<'\n';
    }
    return applied;
}

void ReplayApp::loadSnapshot(const std::filesystem::path& path,NavigationSession& live) {
    live.restoreSnapshot(decodeSnapshot(readBoundedFile(path,maxSnapshotBytes)));
    watching_=recording_=false;
    diagnostic_="Snapshot restored.";
}

void ReplayApp::saveSnapshot(const std::filesystem::path& path,const NavigationSession& live) const {
    writeNewFile(path,encodeSnapshot(displayed(live).captureSnapshot()));
}

void ReplayApp::loadReplay(const std::filesystem::path& path) {
    auto candidate=std::make_unique<ReplayPlayer>(decodeReplay(readBoundedFile(path,maxReplayBytes)));
    while (candidate->next()) {}
    if (candidate->status()==PlaybackStatus::Error) throw std::runtime_error(candidate->error());
    candidate->restart();
    player_=std::move(candidate);
    recorder_.reset();
    watching_=true;
    recording_=false;
    diagnostic_="Loaded and verified all recorded events.";
}

void ReplayApp::saveReplay(const std::filesystem::path& path) const {
    if (!tape()) throw std::logic_error("no recording to save");
    writeNewFile(path,encodeReplay(*tape()));
}

void ReplayApp::startPlayback(bool play) {
    if (player_) player_->restart();
    else if (recorder_) player_=std::make_unique<ReplayPlayer>(recorder_->data());
    else throw std::logic_error("no replay to play");
    recording_=false;
    watching_=true;
    diagnostic_.clear();
    player_->setPlaying(play);
}

void ReplayApp::advancePlayback() {
    if (watching_ && player_->status()==PlaybackStatus::Playing) {
        player_->next();
        if (player_->status()==PlaybackStatus::Error) diagnostic_=player_->error();
    }
}

bool ReplayApp::perform(ReplayAction action,NavigationSession& live) {
    try {
        switch (action) {
        case ReplayAction::None: break;
        case ReplayAction::StartRecording: startRecording(live); break;
        case ReplayAction::StopRecording: recording_=false; break;
        case ReplayAction::LoadSnapshot: loadSnapshot(snapshotPath_.data(),live); return true;
        case ReplayAction::SaveSnapshot:
            saveSnapshot(snapshotPath_.data(),live);
            diagnostic_="Snapshot saved.";
            break;
        case ReplayAction::LoadReplay: loadReplay(replayPath_.data()); return true;
        case ReplayAction::SaveReplay:
            saveReplay(replayPath_.data());
            diagnostic_="Replay saved.";
            break;
        case ReplayAction::PlayFromBase: startPlayback(true); return true;
        case ReplayAction::Restart: startPlayback(false); return true;
        case ReplayAction::Pause: player_->setPlaying(false); break;
        case ReplayAction::Resume: player_->setPlaying(true); break;
        case ReplayAction::Next:
            player_->setPlaying(false);
            player_->next();
            if (player_->status()==PlaybackStatus::Error) diagnostic_=player_->error();
            break;
        case ReplayAction::ReturnLive: watching_=false; return true;
        }
    } catch (const std::exception& error) {
        diagnostic_=error.what();
        std::cerr<<"replay: "<<diagnostic_<<'\n';
    }
    return false;
}

ReplayAction ReplayApp::panel(const NavigationSession& live) {
    using namespace ImGui;
    const auto& sim=displayed(live);
    FrameInfo info;
    observe(info,live);
    using U64=unsigned long long;
    Text("%s   Epoch %llu   Tick %llu",mode(),U64(sim.world().epoch()),U64(sim.world().tick()));
    Text("Phase %s   Manual pause %s",phaseName(sim.phase()),sim.world().paused()?"on":"off");
    Text("Event %llu   Recorded %llu   Verified %llu",U64(info.eventSequence),U64(info.recordedEvents),U64(info.verifiedEvents));
    Text("State hash %016llx",U64(info.simulationHash));
    const char* verification="idle";
    if (recording_) verification="recording event hashes";
    if (watching_) {
        verification="per event";
        if (player_->status()==PlaybackStatus::Error) verification="failed";
        if (player_->status()==PlaybackStatus::Finished) verification="complete";
    }
    Text("Verification: %s",verification);
    ReplayAction selected=ReplayAction::None;
    auto button=[&](const char* label,ReplayAction action,bool enabled=true) {
        BeginDisabled(!enabled);
        if (Button(label)) selected=action;
        EndDisabled();
    };
    button(recording_?"Stop Recording":"Start Recording",recording_?ReplayAction::StopRecording:ReplayAction::StartRecording,!watching_);
    InputTextWithHint("##snapshot-path","Snapshot path",snapshotPath_.data(),snapshotPath_.size());
    button("Save Snapshot",ReplayAction::SaveSnapshot,snapshotPath_[0]);
    SameLine(); button("Load Snapshot",ReplayAction::LoadSnapshot,snapshotPath_[0] && !recording_);
    InputTextWithHint("##replay-path","Replay path",replayPath_.data(),replayPath_.size());
    button("Save Replay",ReplayAction::SaveReplay,replayPath_[0] && tape());
    SameLine(); button("Load Replay",ReplayAction::LoadReplay,replayPath_[0] && !recording_);
    button("Play from Base",ReplayAction::PlayFromBase,tape() && !recording_);
    SameLine(); button("Restart Playback",ReplayAction::Restart,watching_);
    if (watching_) {
        const auto status=player_->status();
        const bool running=status==PlaybackStatus::Playing;
        button(running?"Pause Playback":"Resume Playback",running?ReplayAction::Pause:ReplayAction::Resume,
               running || status==PlaybackStatus::Paused);
        SameLine(); button("Next Event",ReplayAction::Next,running || status==PlaybackStatus::Paused);
        button("Return to Live",ReplayAction::ReturnLive);
    }
    if (!diagnostic_.empty()) TextWrapped("%s",diagnostic_.c_str());
    return selected;
}

void ReplayApp::observe(FrameInfo& info,const NavigationSession& live) const {
    info.replay=true;
    info.replayMode=mode();
    info.recordedEvents=tape()?tape()->events.size():0;
    info.verifiedEvents=watching_?player_->verifiedCount():0;
    info.eventSequence=watching_?info.verifiedEvents:(recording_?info.recordedEvents:0);
    info.simulationHash=stateHash(displayed(live).captureSnapshot());
}

void ReplayApp::finish(const Options& options,const NavigationSession& live) const {
    if (watching_ && player_->status()==PlaybackStatus::Error) throw std::runtime_error(player_->error());
    if (launchRecording_) writeNewFile(options.recordPath,encodeReplay(launchRecording_->data()));
    if (!options.snapshotOut.empty()) saveSnapshot(options.snapshotOut,live);
    FrameInfo info;
    observe(info,live);
    std::cout<<"replay_mode=\""<<mode()<<"\" event_sequence="<<info.eventSequence
             <<" recorded_events="<<info.recordedEvents<<" verified_events="<<info.verifiedEvents
             <<" state_hash="<<std::hex<<info.simulationHash<<std::dec<<'\n';
}
}
