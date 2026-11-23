#pragma once
#include "Selection.h"
#include "tiny2d/DebugDraw.h"
#include "tiny2d/SpriteRenderer.h"
#include "tiny2d/Window.h"
#include <array>
#include <vector>

namespace lab {
enum Pass { World, Extract, Horizontal, Vertical, Output, Overlay, Debug, Preview, Gui, PassCount };
inline constexpr const char* passNames[]={"world","bright","horizontal","vertical","output","hud","debug","preview","gui"};
struct Stats {
    unsigned mask=0, previewMask=0, guiVertices=0, guiIndices=0;
    std::uint64_t vertexBytes=0,indexBytes=0,fontBytes=0,fontUpload=0,worldSprites=0;
    std::array<unsigned,PassCount> passDraws{};
    std::array<std::uint64_t,PassCount> passBytes{};
    unsigned passes = 0, fullscreen = 0;
    std::uint64_t draws = 0, sprites = 0, bytes = 0;
    std::array<double,PassCount> cpuUs{};
    bool complete = false;
};
struct Quad { tiny2d::Vector2 position, size; float angle; tiny2d::Color color; };
struct Picture {
    std::vector<Quad> world, hud, debug;
    void debugLines(const std::vector<tiny2d::DebugLine>&,tiny2d::Color);
    void bounds(const Objects&,std::optional<LabHandle>,bool all,bool selection);
    tiny2d::Camera2D worldCamera{800,450}, hudCamera{800,450};
    void prepare(const Neon&, const Display&, bool paused, tiny2d::Size logical,
                 tiny2d::Size pixels, const Stats& last);
};
}
