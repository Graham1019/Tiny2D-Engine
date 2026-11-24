#pragma once
#include "Occlusion.h"
#include "tiny2d/DebugDraw.h"
#include "tiny2d/SpriteRenderer.h"
#include "tiny2d/Window.h"
#include <array>
#include <vector>

namespace lab {
enum Pass { World, Extract, Horizontal, Vertical, Output, Overlay, Debug, Preview, Gui,
            Albedo, Emission, Light, SceneCompose, Normal, ShadowBatch, ShadowInspect, ParticleEmission,
            ShaderGalleryPass, PassCount };
inline constexpr const char* passNames[]={"world","bright","horizontal","vertical","output","hud","debug","preview","gui",
                                         "albedo","emission","light","scene_compose","normal","shadow_batch","shadow_inspect","particle_emission",
                                         "shader_gallery"};
static_assert(sizeof(passNames)/sizeof(*passNames)==PassCount);
struct ShadowMaskStamp {
    LabHandle source;
    std::uint64_t frame, tick, epoch, generation;
};
struct Stats {
    unsigned mask=0, previewMask=0, guiVertices=0, guiIndices=0;
    std::uint64_t vertexBytes=0,indexBytes=0,fontBytes=0,fontUpload=0,worldSprites=0,lightUniformBytes=0;
    std::array<unsigned,PassCount> passDraws{};
    std::array<std::uint64_t,PassCount> passBytes{};
    unsigned passes = 0, fullscreen = 0;
    std::uint64_t draws = 0, sprites = 0, bytes = 0;
    unsigned activeCasters=0, skippedCasters=0, occluderCount=0, edgeCount=0;
    unsigned maskRebuilds=0, maskDraws=0, lightDraws=0, inspectRebuilds=0, inspectDraws=0;
    std::uint64_t shadowVertexBytes=0;
    double geometryUs=0, maskUs=0, lightSubmitUs=0; // ShadowBatch CPU subintervals.
    std::optional<ShadowMaskStamp> shadowMask; // Only an explicitly rebuilt inspection mask has provenance.
    std::array<double,PassCount> cpuUs{};
    unsigned activeInstances=0, particleDrawCalls=0;
    unsigned tileDrawCalls=0, enemyDrawCalls=0;
    std::uint64_t submittedTiles=0, tileVertexBytes=0, submittedEnemies=0, enemyVertexBytes=0;
    std::uint64_t instanceUploadBytes=0;
    double instanceUploadUs=0;
    bool complete = false;
};
struct Quad { tiny2d::Vector2 position, size; float angle; tiny2d::Color color; };
struct Picture {
    inline static const tiny2d::Vector2 floorOrigin{-320,-144}, floorSize{640,288};
    std::vector<Quad> world, albedo, emission, hud, debug, tiles, enemies;
    LightList lights;
    OccluderList occluders;
    double lightExtractUs=0;
    void debugLines(const std::vector<tiny2d::DebugLine>&,tiny2d::Color);
    void bounds(const Objects&,std::optional<LabHandle>,bool all,bool selection);
    void lightRadii(std::optional<LabHandle> selected={});
    void shadowDebug(const Light2D* source,bool showEdges,bool showGeometry);
    void shadowCaption(std::optional<LabHandle> source,std::uint64_t frame,std::uint64_t tick,
                       std::uint64_t epoch,std::uint64_t generation);
    tiny2d::Camera2D worldCamera{800,450}, hudCamera{800,450};
    void prepare(const Neon&, const Display&, bool paused, tiny2d::Size logical,
                 tiny2d::Size pixels, const Stats& last, std::uint64_t epoch=1);
};
}
