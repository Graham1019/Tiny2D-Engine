#include "ReplayApp.h"
#include "tiny2d/OutputFiles.h"
#include "ShaderAssets.h"
#include "tiny2d_game/SnapshotCodec.h"
#include <imgui.h>
#include <iomanip>
#include <iostream>
#include <random>

namespace lab {
using namespace tiny2d_game;

ReplayApp::ReplayApp(const Options& options,NavigationSession& live) {
    packaged_=tiny2d::insideApplication(tiny2dExecutablePath());
    initialTab_=options.timelineView;
    if (!options.snapshotIn.empty()) loadSnapshot(options.snapshotIn,live);
    if (!options.replayPath.empty()) loadReplay(options.replayPath);
    if (!options.branchIn.empty()) loadBranch(options.branchIn);
    if (watching_) forward_=true;
    if (options.seekEvent) seek(*options.seekEvent);
    if (options.forkAt) {
        seek(*options.forkAt);
        fork(options.branchId);
        if (!options.branchOut.empty()) launchBranch_=timeline_->child();
    }
    if (!options.recordPath.empty()) {
        startRecording(live);
        launchRecording_=recorder_;
    }
}

const NavigationSession& ReplayApp::displayed(const NavigationSession& live) const {
    return watching_ ? timeline_->session() : live;
}

const ReplayData* ReplayApp::tape() const {
    if (watching_) return &timeline_->data();
    if (recorder_) return &recorder_->data();
    return timeline_ ? &timeline_->data() : nullptr;
}

PlaybackStatus ReplayApp::status() const {
    if (failed_) return PlaybackStatus::Error;
    if (timeline_->cursor()==timeline_->data().events.size()) return PlaybackStatus::Finished;
    return forward_?PlaybackStatus::Playing:PlaybackStatus::Paused;
}

const char* ReplayApp::mode() const {
    if (!watching_) return recording_ ? "Recording" : "Live";
    if (editingBranch()) return "Branch live";
    switch (status()) {
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
    timeline_.reset();
    watching_=false;
    recording_=true;
    diagnostic_.clear();
}

bool ReplayApp::dispatch(NavigationSession& live,const ReplayCommand& command) {
    if (playback()) return false;
    const auto epoch=displayed(live).world().epoch();
    bool applied=false;
    if (editingBranch()) {
        try { applied=timeline_->append(command); }
        catch (const std::length_error& error) {
            diagnostic_=error.what();
            return false;
        }
    } else if (recording_) {
        applied=recorder_->apply(live,command);
        if (recorder_->full()) {
            recording_=false;
            diagnostic_="Recording budget reached. The recorded events are available to save.";
            std::cerr<<diagnostic_<<'\n';
        }
    } else applied=dispatchEvent(live,command);
    if (applied && displayed(live).world().epoch()!=epoch) ++revision_;
    return applied;
}

bool ReplayApp::dispatchAtRevision(NavigationSession& live,const ReplayCommand& command,std::uint64_t observed) {
    return observed==revision_ && dispatch(live,command);
}

void ReplayApp::loadSnapshot(const std::filesystem::path& path,NavigationSession& live) {
    live.restoreSnapshot(decodeSnapshot(readBoundedFile(path,maxSnapshotBytes)));
    watching_=recording_=false;
    ++revision_;
    dragging_=false;
    diagnostic_="Snapshot restored.";
}

void ReplayApp::saveSnapshot(const std::filesystem::path& path,const NavigationSession& live) const {
    if (packaged_) tiny2d::requireExternalOutput(path);
    writeNewFile(path,encodeSnapshot(displayed(live).captureSnapshot()));
}

void ReplayApp::publish(std::unique_ptr<TimelineView> candidate) {
    timeline_=std::move(candidate);
    recorder_.reset();
    watching_=true;
    recording_=forward_=failed_=dragging_=false;
    ++revision_;
    diagnostic_="Loaded and verified all recorded events.";
}

void ReplayApp::loadReplay(const std::filesystem::path& path) {
    publish(std::make_unique<TimelineView>(decodeReplay(readBoundedFile(path,maxReplayBytes))));
}

void ReplayApp::loadBranch(const std::filesystem::path& path) {
    publish(std::make_unique<TimelineView>(decodeBranch(readBoundedFile(path,maxBranchBytes))));
}

void ReplayApp::saveReplay(const std::filesystem::path& path) const {
    if (!tape()) throw std::logic_error("no recording to save");
    if (packaged_) tiny2d::requireExternalOutput(path);
    writeNewFile(path,encodeReplay(*tape()));
}

void ReplayApp::startPlayback(bool play) {
    if (!timeline_) {
        if (!recorder_) throw std::logic_error("no replay to play");
        timeline_=std::make_unique<TimelineView>(recorder_->data());
    }
    seek(0);
    watching_=true;
    recording_=false;
    forward_=play;
}

void ReplayApp::seek(std::size_t cursor) {
    timeline_->seek(cursor);
    forward_=failed_=dragging_=false;
    diagnostic_.clear();
    ++revision_;
}

void ReplayApp::fork(std::uint64_t id) {
    if (!id) {
        std::random_device random;
        do {
            id=(std::uint64_t(random())<<32) | random();
        } while (!id || id==timeline_->branchId() || id==timeline_->index().recordingId());
    }
    timeline_->fork(id);
    forward_=failed_=dragging_=false;
    diagnostic_.clear();
    ++revision_;
}

void ReplayApp::nextEvent() {
    const auto epoch=timeline_->session().world().epoch();
    try {
        if (timeline_->next() && timeline_->session().world().epoch()!=epoch) ++revision_;
    } catch (const std::exception& error) {
        forward_=false;
        failed_=true;
        diagnostic_=error.what();
    }
}

void ReplayApp::advancePlayback() {
    if (playback() && status()==PlaybackStatus::Playing) nextEvent();
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
            diagnostic_="Saved snapshot to "+std::filesystem::absolute(snapshotPath_.data()).string();
            break;
        case ReplayAction::LoadReplay: loadReplay(replayPath_.data()); return true;
        case ReplayAction::SaveReplay:
            saveReplay(replayPath_.data());
            diagnostic_="Saved replay to "+std::filesystem::absolute(replayPath_.data()).string();
            break;
        case ReplayAction::PlayFromBase: startPlayback(true); return true;
        case ReplayAction::Restart: startPlayback(false); return true;
        case ReplayAction::Pause: forward_=false; break;
        case ReplayAction::Resume: forward_=true; break;
        case ReplayAction::Next:
            forward_=false;
            if (!failed_) nextEvent();
            break;
        case ReplayAction::Seek: seek(std::size_t(seekTarget_)); return true;
        case ReplayAction::Previous:
            if (timeline_->cursor()) seek(timeline_->cursor()-1);
            return true;
        case ReplayAction::End: seek(timeline_->data().events.size()); return true;
        case ReplayAction::Fork: fork(0); return true;
        case ReplayAction::Parent:
            timeline_->inspectParent();
            break;
        case ReplayAction::Child: timeline_->inspectChild(); break;
        case ReplayAction::ResumeChild: timeline_->resumeChild(); break;
        case ReplayAction::LoadBranch: loadBranch(branchPath_.data()); return true;
        case ReplayAction::SaveBranch:
            if (const auto* child=timeline_->selectedBranch()) {
                if (packaged_) tiny2d::requireExternalOutput(branchPath_.data());
                writeNewFile(branchPath_.data(),encodeBranch(*child));
                diagnostic_="Saved branch to "+std::filesystem::absolute(branchPath_.data()).string();
            }
            break;
        case ReplayAction::ReturnLive: watching_=false; break;
        }
        if (action==ReplayAction::Parent || action==ReplayAction::Child || action==ReplayAction::ResumeChild ||
            action==ReplayAction::ReturnLive) {
            forward_=failed_=dragging_=false;
            diagnostic_.clear();
            ++revision_;
            return true;
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
    if (playback()) {
        verification="per event";
        if (status()==PlaybackStatus::Error) verification="failed";
        if (status()==PlaybackStatus::Finished) verification="complete";
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
    if (playback()) {
        const auto state=status();
        const bool running=state==PlaybackStatus::Playing;
        button(running?"Pause Playback":"Resume Playback",running?ReplayAction::Pause:ReplayAction::Resume,
               running || state==PlaybackStatus::Paused);
        SameLine(); button("Next Event",ReplayAction::Next,running || state==PlaybackStatus::Paused);
    }
    if (watching_) button("Return to Live",ReplayAction::ReturnLive);
    if (!diagnostic_.empty()) TextWrapped("%s",diagnostic_.c_str());
    return selected;
}

ReplayAction ReplayApp::timelinePanel(const NavigationSession& live) {
    using namespace ImGui;
    using U64=unsigned long long;
    ReplayAction selected=ReplayAction::None;
    auto button=[&](const char* label,ReplayAction action,bool enabled=true) {
        BeginDisabled(!enabled);
        if (Button(label)) selected=action;
        EndDisabled();
    };
    if (!watching_) {
        button("Open recorded timeline",ReplayAction::Restart,tape() && !recording_);
    } else {
        const auto& view=*timeline_;
        const auto cursor=view.cursor(), count=view.data().events.size();
        const auto& session=displayed(live);
        Text("%s   %s",mode(),phaseName(session.phase()));
        Text("Branch %016llx",U64(view.branchId()));
        if (const auto* branch=view.selectedBranch()) {
            const auto& meta=branch->metadata();
            Text("Parent %016llx   Fork %llu",U64(meta.parentId),U64(meta.forkCursor));
        } else TextDisabled("Root recording");
        Text("Cursor %zu / %zu    Epoch %llu   Tick %llu",cursor,count,
             U64(session.world().epoch()),U64(session.world().tick()));
        Text("%s   Hash %016llx",cursor?eventName(view.data().events[cursor-1].command.type()):"Base",
             U64(stateHash(session.captureSnapshot())));
        Text("Resident checkpoints %zu   %.1f KiB",view.checkpointCount(),view.checkpointBytes()/1024.);
        if (!view.editing()) {
            const auto& checkpoints=view.index().checkpoints();
            const auto& seek=view.index().lastSeek();
            Text("Last seek: checkpoint %zu + %zu events",seek.checkpointCursor,seek.replayedEvents);
            if (!dragging_) seekTarget_=int(cursor);
            SetNextItemWidth(-1);
            SliderInt("##event-seek",&seekTarget_,0,int(count),"Event %d",ImGuiSliderFlags_AlwaysClamp);
            if (IsItemActivated()) { dragging_=true; selected=ReplayAction::Pause; }
            if (IsItemDeactivated()) {
                dragging_=false;
                if (std::size_t(seekTarget_)!=cursor) selected=ReplayAction::Seek;
            } else if (!IsItemActive()) dragging_=false;
            const auto left=GetCursorScreenPos();
            const float span=std::max(1.f,GetContentRegionAvail().x-1);
            auto* draw=GetWindowDrawList();
            draw->AddLine({left.x,left.y+3},{left.x+span,left.y+3},IM_COL32(71,91,97,255));
            for (const auto& cp:checkpoints) {
                const float x=left.x+(count?span*float(cp.cursor)/float(count):0);
                draw->AddLine({x,left.y},{x,left.y+6},IM_COL32(104,227,206,255),2.f);
            }
            Dummy({span,7});
            button("Start",ReplayAction::Restart,cursor>0); SameLine();
            button("Prev",ReplayAction::Previous,cursor>0); SameLine();
            button("Next",ReplayAction::Next,cursor<count); SameLine();
            button("End",ReplayAction::End,cursor<count);
            SameLine();
            const bool running=status()==PlaybackStatus::Playing;
            button(running?"Pause":"Play forward",running?ReplayAction::Pause:ReplayAction::Resume,cursor<count);
        }
        if (!view.hasChild()) {
            button("Fork / Resume from Here",ReplayAction::Fork);
        } else {
            button("View parent",ReplayAction::Parent,view.childSelected()); SameLine();
            button("Inspect child",ReplayAction::Child,!view.childSelected() || view.editing());
            if (view.childSelected() && !view.editing()) {
                SameLine(); button("Resume child",ReplayAction::ResumeChild,cursor==count);
            }
        }
        button("Return to Live",ReplayAction::ReturnLive);
    }
    InputTextWithHint("##branch-path","Branch file path",branchPath_.data(),branchPath_.size());
    button("Save Branch",ReplayAction::SaveBranch,branchPath_[0] && watching_ && timeline_->selectedBranch());
    SameLine(); button("Load Branch",ReplayAction::LoadBranch,branchPath_[0] && !recording_);
    if (!diagnostic_.empty()) TextWrapped("%s",diagnostic_.c_str());
    return selected;
}

void ReplayApp::observe(FrameInfo& info,const NavigationSession& live) const {
    info.replay=true;
    info.replayMode=mode();
    info.recordedEvents=tape()?tape()->events.size():0;
    info.verifiedEvents=playback()?timeline_->cursor():0;
    info.eventSequence=watching_?timeline_->cursor():(recording_?info.recordedEvents:0);
    info.simulationHash=stateHash(displayed(live).captureSnapshot());
    if (watching_) {
        info.timeline=true;
        info.branchId=timeline_->branchId();
        if (const auto* branch=timeline_->selectedBranch()) {
            info.parentBranch=branch->metadata().parentId;
            info.forkCursor=branch->metadata().forkCursor;
        }
        info.checkpointCount=timeline_->checkpointCount();
        info.checkpointBytes=timeline_->checkpointBytes();
        info.eventType=timeline_->cursor()?eventName(timeline_->data().events[timeline_->cursor()-1].command.type()):"Base";
    }
}

void ReplayApp::finish(const Options& options,const NavigationSession& live) const {
    if (playback() && failed_) throw std::runtime_error(diagnostic_);
    if (launchBranch_) writeNewFile(options.branchOut,encodeBranch(*launchBranch_));
    if (launchRecording_) writeNewFile(options.recordPath,encodeReplay(launchRecording_->data()));
    if (!options.snapshotOut.empty()) saveSnapshot(options.snapshotOut,live);
    FrameInfo info;
    observe(info,live);
    std::cout<<"replay_mode=\""<<mode()<<"\" event_sequence="<<info.eventSequence
             <<" recorded_events="<<info.recordedEvents<<" verified_events="<<info.verifiedEvents
             <<" state_hash="<<std::hex<<info.simulationHash<<std::dec<<'\n';
}
}
