#include "ShowcaseProcess.h"
#include <algorithm>
#include <cerrno>
#include <csignal>
#include <crt_externs.h>
#include <spawn.h>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <sys/wait.h>

namespace lab {
namespace fs=std::filesystem;

const std::array<Exhibit,8>& exhibits() {
    static constexpr std::array<Exhibit,8> list{{
        {ExhibitId::Neon,"neon","Neon Lighting","Fire across the relay field with normal-mapped light and moving shadows.",
            "WASD move   Space fire   B bloom   1-4 buffers","neon.png"},
        {ExhibitId::Particles,"particles","Particle Fields","Explore a steady field of 8,192 instanced particles and local forces.",
            "Drag in the scene to move the force field.   B bloom","particles.png"},
        {ExhibitId::Shaders,"shaders","Shader Gallery","Explore three fragment programs with the gallery Inspector.",
            "Select an effect in the Inspector.   F5 reload   1-4 buffers","shaders.png"},
        {ExhibitId::Map,"map","Seeded Tilemap","Walk a connected procedural dungeon and inspect swept tile collisions.",
            "WASD move   P pause   Period step   R reset","map.png"},
        {ExhibitId::Navigation,"navigation","Enemy Navigation","Follow the shared distance field and committed enemy routes.",
            "WASD move   P pause   Click an offered upgrade","navigation.png"},
        {ExhibitId::Replay,"replay","Replay & Timeline","Revisit the first upgrade offer, seek by event or fork a new choice.",
            "Drag the timeline and release to seek.   Fork to play","replay.png"},
        {ExhibitId::Classic,"survivor-classic","Survivor Classic","Play the original arena with automatic attacks and five difficulty levels.",
            "1-5 difficulty   Enter start   WASD move   R restart","classic.png"},
        {ExhibitId::Enhanced,"survivor-enhanced","Survivor Enhanced","Play the same survival rules with the enhanced presentation.",
            "1-5 difficulty   Enter start   WASD move   R restart","enhanced.png"}
    }};
    return list;
}
const Exhibit& findExhibit(std::string_view key) {
    const auto& list=exhibits();
    const auto it=std::find_if(list.begin(),list.end(),[&](const Exhibit& entry){return entry.key==key;});
    if (it==list.end()) throw std::invalid_argument("unknown showcase exhibit: "+std::string(key));
    return *it;
}
std::vector<ExhibitId> tourVisits(std::string_view list) {
    std::vector<ExhibitId> result;
    if (list=="all") {
        for (const auto& entry:exhibits()) result.push_back(entry.id);
        return result;
    }
    while (true) {
        const auto comma=list.find(',');
        result.push_back(findExhibit(list.substr(0,comma)).id);
        if (result.size()>64) throw std::invalid_argument("showcase tour exceeds 64 visits");
        if (comma==list.npos) break;
        list.remove_prefix(comma+1);
    }
    return result;
}

LaunchCommand exhibitCommand(ExhibitId id,const fs::path& actualLab,const fs::path& replayFile,
                             unsigned childFrames,const fs::path& outputStem,const fs::path& shaderWorkspace) {
    LaunchCommand launch{actualLab,{}};
    auto& args=launch.arguments;
    const bool survivor=id==ExhibitId::Classic || id==ExhibitId::Enhanced;
    switch (id) {
    case ExhibitId::Neon:
        args={"--scene","neon","--lighting","on","--normal-mapping","on","--shadows","on"};
        break;
    case ExhibitId::Particles:
        args={"--scene","particles","--particle-capacity","8192","--particle-steady","8192"};
        if (childFrames) args.insert(args.end(),{"--particle-script","demo"});
        break;
    case ExhibitId::Shaders:
        args={"--scene","shaders","--effect","aurora"};
        if (!shaderWorkspace.empty()) args.insert(args.end(),{"--shader-workspace",fs::absolute(shaderWorkspace).string()});
        break;
    case ExhibitId::Map:
        args={"--scene","map"};
        if (childFrames) args.insert(args.end(),{"--map-script","walk"});
        break;
    case ExhibitId::Navigation:
        args={"--scene","navigation","--nav-debug","path","--nav-enemies","16"};
        if (childFrames) args.insert(args.end(),{"--nav-script","walk"});
        break;
    case ExhibitId::Replay:
        args={"--scene","replay","--replay",replayFile.string(),"--seek-event","600","--timeline-view","on","--inspector","on"};
        break;
    case ExhibitId::Classic:
    case ExhibitId::Enhanced: {
        const auto dir=actualLab.parent_path();
        if (dir.filename()=="MacOS" && dir.parent_path().filename()=="Contents")
            launch.executable=dir.parent_path().parent_path().parent_path()/"Tiny2D Survivor.app/Contents/MacOS/Tiny2DSurvivor";
        else launch.executable=dir.parent_path()/"Tiny2DSurvivor.app/Contents/MacOS/Tiny2DSurvivor";
        args={"--presentation",id==ExhibitId::Classic?"classic":"enhanced"};
        break;
    }
    }
    if (childFrames) args.insert(args.end(),{"--frames",std::to_string(childFrames)});
    if (!outputStem.empty()) {
        const auto stem=outputStem.string();
        args.insert(args.end(),{"--capture",stem+".png","--metrics",stem+".csv"});
        if (survivor) args.insert(args.end(),{"--trace",stem+".trace.csv"});
    }
    return launch;
}

ExhibitProcess::~ExhibitProcess() { close(); }

pid_t ExhibitProcess::start(const LaunchCommand& command) {
    if (running()) throw std::logic_error("an exhibit is already running");
    auto executable=command.executable.string();
    std::vector<char*> argv{executable.data()};
    for (const auto& token:command.arguments) argv.push_back(const_cast<char*>(token.c_str()));
    argv.push_back(nullptr);
    pid_t child=-1;
    const int error=posix_spawn(&child,executable.c_str(),nullptr,nullptr,argv.data(),*_NSGetEnviron());
    if (error) throw std::system_error(error,std::generic_category(),"launch "+executable);
    pid_=child;
    return pid_;
}

std::optional<ChildExit> ExhibitProcess::poll() {
    if (!running()) return {};
    int status=0;
    const auto result=::waitpid(pid_,&status,WNOHANG);
    if (!result || (result<0 && errno==EINTR)) return {};
    if (result<0) {
        const int error=errno;
        if (error==ECHILD) pid_=-1;
        throw std::system_error(error,std::generic_category(),"wait for exhibit");
    }
    pid_=-1;
    if (WIFEXITED(status)) return ChildExit{WEXITSTATUS(status),0};
    const auto signal=WTERMSIG(status);
    return ChildExit{128+signal,signal};
}

void ExhibitProcess::close() noexcept {
    if (!running()) return;
    // Only this child is signalled. Until waitpid reaps it, its PID cannot be reused.
    ::kill(pid_,SIGTERM);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(500);
    int status=0;
    while (std::chrono::steady_clock::now()<deadline) {
        const auto result=::waitpid(pid_,&status,WNOHANG);
        if (result==pid_ || (result<0 && errno==ECHILD)) { pid_=-1; return; }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ::kill(pid_,SIGKILL);
    while (::waitpid(pid_,&status,0)<0 && errno==EINTR) {}
    pid_=-1;
}
}
