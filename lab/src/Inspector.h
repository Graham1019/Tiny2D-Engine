#pragma once
#include "Pipeline.h"
#include <imgui.h>

namespace lab {
struct ToolState {
    bool visible=true, timing=true, previews=false, bounds=false, outline=true;
    std::optional<LabHandle> selected;
};
struct RoutedInput {
    Commands commands;
    Keys keys;
    bool captureKeyboard=false;
    std::optional<tiny2d::Vector2> click;
};
class Inspector {
public:
    Inspector(tiny2d::Window&,bool finite);
    ~Inspector();
    Inspector(const Inspector&)=delete;
    Inspector& operator=(const Inspector&)=delete;
    RoutedInput build(ToolState&,const Neon&,std::uint64_t epoch,const Display&,bool paused,
                      const History&,const tiny2d::QueryCounts&,const std::string& capability,bool focused);
    unsigned finish(const ToolState&,Pipeline&,std::uint64_t frame,std::uint64_t tick);
    void submit(Stats&);
    std::uint64_t atlasBytes() const { return atlasBytes_; }
    void discard();
    void suspend();
private:
    GLFWwindow* window_;
    std::uint64_t atlasBytes_=0;
    bool finite_,active_=false,ready_=false,focusGate_=false,f2Down_=false,mouseBlocked_=false;
    bool first_=true;
    char filter_[64]{};
    void requireOwner() const;
    std::string renderer_,version_;
};
}
