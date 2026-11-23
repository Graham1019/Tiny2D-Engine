#pragma once
#include "Lighting.h"
#include "tiny2d/DebugDraw.h"
#include "tiny2d/SpriteRenderer.h"
#include "tiny2d/Window.h"
#include <array>
#include <vector>

namespace lab {
enum Pass { World, Extract, Horizontal, Vertical, Output, Overlay, Debug, Preview, Gui,
            Albedo, Emission, Light, SceneCompose, Normal, PassCount };
inline constexpr const char* passNames[]={"world","bright","horizontal","vertical","output","hud","debug","preview","gui",
                                         "albedo","emission","light","scene_compose","normal"};
static_assert(sizeof(passNames)/sizeof(*passNames)==PassCount);
struct Stats {
    unsigned mask=0, previewMask=0, guiVertices=0, guiIndices=0;
    std::uint64_t vertexBytes=0,indexBytes=0,fontBytes=0,fontUpload=0,worldSprites=0,lightUniformBytes=0;
    std::array<unsigned,PassCount> passDraws{};
    std::array<std::uint64_t,PassCount> passBytes{};
    unsigned passes = 0, fullscreen = 0;
    std::uint64_t draws = 0, sprites = 0, bytes = 0;
    std::array<double,PassCount> cpuUs{};
    bool complete = false;
};
struct Quad { tiny2d::Vector2 position, size; float angle; tiny2d::Color color; };
struct Picture {
    inline static const tiny2d::Vector2 floorOrigin{-320,-144}, floorSize{640,288};
    std::vector<Quad> world, albedo, emission, hud, debug;
    LightList lights;
    double lightExtractUs=0;
    void debugLines(const std::vector<tiny2d::DebugLine>&,tiny2d::Color);
    void bounds(const Objects&,std::optional<LabHandle>,bool all,bool selection);
    void lightRadii(std::optional<LabHandle> selected={});
    tiny2d::Camera2D worldCamera{800,450}, hudCamera{800,450};
    void prepare(const Neon&, const Display&, bool paused, tiny2d::Size logical,
                 tiny2d::Size pixels, const Stats& last, std::uint64_t epoch=1);
};
}
