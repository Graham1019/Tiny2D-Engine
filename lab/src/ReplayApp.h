#pragma once
#include "Observation.h"
#include "tiny2d_game/Replay.h"
#include <array>
#include <filesystem>
#include <memory>

namespace lab {

enum class ReplayAction {
    None, StartRecording, StopRecording, SaveSnapshot, LoadSnapshot, SaveReplay, LoadReplay,
    PlayFromBase, Restart, Pause, Resume, Next, ReturnLive
};

// Owns the displayed playback candidate separately from the live NavigationSession.
class ReplayApp {
public:
    ReplayApp(const Options&,tiny2d_game::NavigationSession& live);
    bool playback() const { return watching_; }
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
private:
    const tiny2d_game::ReplayData* tape() const;
    const char* mode() const;
    void startRecording(const tiny2d_game::NavigationSession&);
    void startPlayback(bool play);
    void saveSnapshot(const std::filesystem::path&,const tiny2d_game::NavigationSession&) const;
    void saveReplay(const std::filesystem::path&) const;
    std::shared_ptr<tiny2d_game::ReplayRecorder> recorder_;
    // --record names the recording started on launch, even after another tape is opened.
    std::shared_ptr<const tiny2d_game::ReplayRecorder> launchRecording_;
    std::unique_ptr<tiny2d_game::ReplayPlayer> player_;
    bool recording_=false,watching_=false;
    std::string diagnostic_;
    std::array<char,1024> snapshotPath_{},replayPath_{};
};
}
