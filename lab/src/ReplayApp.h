#pragma once
#include "Observation.h"
#include "TimelineView.h"
#include <array>
#include <filesystem>
#include <memory>

namespace lab {

enum class ReplayAction {
    None, StartRecording, StopRecording, SaveSnapshot, LoadSnapshot, SaveReplay, LoadReplay,
    PlayFromBase, Restart, Pause, Resume, Next, ReturnLive,
    Seek, Previous, End, Fork, Parent, Child, ResumeChild, LoadBranch, SaveBranch
};

// Owns the displayed playback candidate separately from the live NavigationSession.
class ReplayApp {
public:
    ReplayApp(const Options&,tiny2d_game::NavigationSession& live);
    bool playback() const { return watching_ && !timeline_->editing(); }
    bool editingBranch() const { return watching_ && timeline_->editing(); }
    std::uint64_t revision() const { return revision_; }
    bool dispatchAtRevision(tiny2d_game::NavigationSession&,const tiny2d_game::ReplayCommand&,std::uint64_t);
    bool selectTimelineTab() { bool show=initialTab_; initialTab_=false; return show; }
    ReplayAction timelinePanel(const tiny2d_game::NavigationSession&);
    void seek(std::size_t);
    void fork(std::uint64_t);
    const tiny2d_game::NavigationSession& displayed(const tiny2d_game::NavigationSession& live) const;
    bool dispatch(tiny2d_game::NavigationSession&,const tiny2d_game::ReplayCommand&);
    ReplayAction panel(const tiny2d_game::NavigationSession&);
    // Returns true when the runtime instance changed, even for the same historical epoch.
    bool perform(ReplayAction,tiny2d_game::NavigationSession&);
    void advancePlayback();
    void observe(FrameInfo&,const tiny2d_game::NavigationSession&) const;
    void finish(const Options&,const tiny2d_game::NavigationSession&) const;
    const std::string& diagnostic() const { return diagnostic_; }
    void loadSnapshot(const std::filesystem::path&,tiny2d_game::NavigationSession&);
    void loadReplay(const std::filesystem::path&);
    void loadBranch(const std::filesystem::path&);
private:
    const tiny2d_game::ReplayData* tape() const;
    const char* mode() const;
    tiny2d_game::PlaybackStatus status() const;
    void publish(std::unique_ptr<TimelineView>);
    void nextEvent();
    void startRecording(const tiny2d_game::NavigationSession&);
    void startPlayback(bool play);
    void saveSnapshot(const std::filesystem::path&,const tiny2d_game::NavigationSession&) const;
    void saveReplay(const std::filesystem::path&) const;
    std::shared_ptr<tiny2d_game::ReplayRecorder> recorder_;
    // --record names the recording started on launch, even after another tape is opened.
    std::shared_ptr<const tiny2d_game::ReplayRecorder> launchRecording_;
    std::shared_ptr<const tiny2d_game::ReplayBranch> launchBranch_;
    std::unique_ptr<TimelineView> timeline_;
    bool forward_=false, failed_=false, initialTab_=false, dragging_=false;
    std::uint64_t revision_=0;
    int seekTarget_=0;
    bool recording_=false,watching_=false,packaged_=false;
    std::string diagnostic_;
    std::array<char,1024> snapshotPath_{},replayPath_{},branchPath_{};
};
}
