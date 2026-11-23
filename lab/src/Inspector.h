#pragma once
#include "Pipeline.h"
#include <imgui.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <bitset>
#include <stdexcept>

namespace lab {
class PreviewAllocationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
struct ToolState {
    bool visible=true, timing=true, previews=false, bounds=false, outline=true;
    std::optional<LabHandle> selected;
    bool lightRadii=false;
    bool occluderEdges=false, shadowGeometry=false, followPlayer=true;
    std::optional<LabHandle> shadowSource;
    void resolveShadow(const LightList&,const ShadowCasters&,std::uint64_t epoch);
};
struct RoutedInput {
    Commands commands;
    Keys keys;
    bool captureKeyboard=false;
    bool freshFocus=false; // Keyboard snapshot contains only events received after GLFW focus gain.
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
    unsigned finish(ToolState&,Pipeline&,const FrameInfo&,const Picture&,const ShadowCasters& = {});
    void submit(Stats&);
    std::uint64_t atlasBytes() const { return atlasBytes_; }
    void discard();
    void suspend();
private:
    GLFWwindow* window_;
    std::uint64_t atlasBytes_=0;
    bool finite_,active_=false,ready_=false,focusGate_=false,f2Down_=false,mouseBlocked_=false;
    bool first_=true, freshFocus_=false, restoreCursor_=false;
    bool lightingTab_=false, shadowTab_=false;
    std::bitset<GLFW_KEY_LAST+1> heldAtLoss_;
    static void focusChanged(GLFWwindow*,int);
    static void keyAfterFocus(GLFWwindow*,int key,int scancode,int action,int mods);
    char filter_[64]{};
    void requireOwner() const;
    std::string renderer_,version_;
};
}
