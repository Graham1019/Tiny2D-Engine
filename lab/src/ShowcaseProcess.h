#pragma once
#include <array>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <sys/types.h>

namespace lab {
enum class ExhibitId { Neon, Particles, Shaders, Map, Navigation, Replay, Classic, Enhanced };
struct Exhibit {
    ExhibitId id;
    const char* key;
    const char* title;
    const char* feature;
    const char* controls;
    const char* preview;
};
const std::array<Exhibit,8>& exhibits();
const Exhibit& findExhibit(std::string_view key);
std::vector<ExhibitId> tourVisits(std::string_view list);

struct LaunchCommand {
    std::filesystem::path executable;
    std::vector<std::string> arguments;
};
// actualLab is the executable resolved by dyld; argv entries are never shell text.
LaunchCommand exhibitCommand(ExhibitId,const std::filesystem::path& actualLab,
    const std::filesystem::path& replayFile,unsigned childFrames=0,const std::filesystem::path& outputStem={},
    const std::filesystem::path& shaderWorkspace={});

struct ChildExit { int code; int signal; };
class ExhibitProcess {
public:
    ExhibitProcess()=default;
    ~ExhibitProcess();
    ExhibitProcess(const ExhibitProcess&)=delete;
    ExhibitProcess& operator=(const ExhibitProcess&)=delete;
    pid_t start(const LaunchCommand&);
    std::optional<ChildExit> poll();
    void close() noexcept;
    bool running() const { return pid_>0; }
private:
    pid_t pid_=-1;
};
}
