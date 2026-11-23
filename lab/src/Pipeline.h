#pragma once
#include "Observation.h"
#include <functional>
#include "tiny2d/BatchRenderer.h"
#include "tiny2d/RenderTarget2D.h"
#include <chrono>

namespace lab {
class ParticleRenderer;
struct ParticleRenderSnapshot;
using Clock = std::chrono::steady_clock;
inline double micros(Clock::time_point start) {
    return std::chrono::duration<double,std::micro>(Clock::now()-start).count();
}
struct PreviewView { unsigned texture; tiny2d::Size source, thumbnail; std::uint64_t generation; };
struct DrawTools {
    unsigned previews=0; // Bit per View in enum order.
    Timer* timer=nullptr;
    std::function<void(Stats&)> gui;
    std::optional<LabHandle> shadowSource;
    std::uint64_t frame=0, tick=0, epoch=0;
    const ParticleRenderSnapshot* particles=nullptr; // Borrowed only for this synchronous draw.
};
class Pipeline {
public:
    Pipeline();
    ~Pipeline();
    // Called at a frame boundary, before borrowing any attachment. Zero suspends.
    bool resize(tiny2d::Size,bool lighting=false,bool normalMapping=false,bool shadows=false);
    tiny2d::Size lightSize() const; // Resident HDR extent, including retained same-size targets.
    tiny2d::Size normalSize() const;
    std::uint64_t materialBytes() const; // Immutable linear RGBA8 tile, independent of target size.
    double normalPrepareUs() const;
    tiny2d::Size size() const;
    std::uint64_t storage() const;
    std::uint64_t generation() const { return generation_; }
    std::uint64_t previewBytes() const;
    std::uint64_t shadowBytes() const; // Resident, full-resolution RGBA8 mask.
    std::array<PreviewView,4> previews();
    PreviewView lightPreview(); // Enabled frame only. Discard borrows before the next resize.
    PreviewView normalPreview();
    PreviewView shadowPreview();
    void draw(const Picture&, const Display&, Stats& attempt,const DrawTools& tools={});
private:
    friend struct PipelineProbe; // Driver-backed tests inspect attachment lifetimes at idle boundaries.
    using Checkpoint=std::function<void(const tiny2d::RenderTarget2D&)>;
    Checkpoint allocationCheckpoint_; // Optional observer after each attachment allocation.
    using BufferAllocation=std::function<void(std::size_t)>;
    BufferAllocation allocateShadowBuffer_; // Driver failure injection for the initial bound VBO allocation.
    using Thumbnails=std::array<tiny2d::RenderTarget2D,4>;
    struct LightingTargets {
        tiny2d::RenderTarget2D albedo, emission, light;
        std::unique_ptr<tiny2d::RenderTarget2D> thumbnail;
        LightingTargets(int w,int h,const Checkpoint&);
    };
    struct NormalTargets {
        tiny2d::RenderTarget2D buffer;
        std::unique_ptr<tiny2d::RenderTarget2D> thumbnail;
        NormalTargets(int w,int h,const Checkpoint&);
    };
    struct NormalAssets; // Compiled programs and uploaded material shared across framebuffer sizes.
    struct ShadowAssets;
    struct ShadowTargets {
        tiny2d::RenderTarget2D buffer;
        std::unique_ptr<tiny2d::RenderTarget2D> thumbnail;
        ShadowTargets(int w,int h,const Checkpoint&);
    };
    struct Targets {
        tiny2d::RenderTarget2D scene, bright, ping, pong;
        std::unique_ptr<LightingTargets> lighting;
        std::unique_ptr<NormalTargets> normal;
        std::unique_ptr<ShadowTargets> shadow;
        std::unique_ptr<Thumbnails> thumbnails;
        void allocatePreviews(const Checkpoint&);
        unsigned previewCount() const;
        Targets(int width,int height,const Checkpoint&);
    };
    std::unique_ptr<Targets> targets_;
    std::unique_ptr<NormalAssets> normalAssets_;
    std::unique_ptr<ShadowAssets> shadowAssets_;
    std::unique_ptr<ParticleRenderer> particleRenderer_;
    std::function<std::unique_ptr<ParticleRenderer>()> createParticleRenderer_; // Driver failure fixture.
    std::uint64_t generation_=0;
    bool drawable_ = false, lightingEnabled_ = false, normalEnabled_ = false, shadowEnabled_ = false;
    tiny2d::detail::GLObject triangle_{tiny2d::detail::ObjectKind::VertexArray};
    tiny2d::ShaderProgram bright_, blur_, display_, light_, compose_;
    tiny2d::Texture2D white_;
    tiny2d::BatchRenderer batch_{2048,1};
    void sprites(const std::vector<Quad>&, const tiny2d::Camera2D&, Stats&);
};
}
