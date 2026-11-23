#pragma once
#include "Observation.h"
#include <functional>
#include "tiny2d/BatchRenderer.h"
#include "tiny2d/RenderTarget2D.h"
#include <chrono>

namespace lab {
using Clock = std::chrono::steady_clock;
inline double micros(Clock::time_point start) {
    return std::chrono::duration<double,std::micro>(Clock::now()-start).count();
}
struct PreviewView { unsigned texture; tiny2d::Size source, thumbnail; std::uint64_t generation; };
struct DrawTools {
    unsigned previews=0; // Bit per View in enum order.
    Timer* timer=nullptr;
    std::function<void(Stats&)> gui;
};
class Pipeline {
public:
    Pipeline();
    ~Pipeline();
    // Called at a frame boundary, before borrowing any attachment. Zero suspends.
    bool resize(tiny2d::Size);
    tiny2d::Size size() const;
    std::uint64_t storage() const;
    std::uint64_t generation() const { return generation_; }
    std::uint64_t previewBytes() const;
    std::array<PreviewView,4> previews();
    void draw(const Picture&, const Display&, Stats& attempt,const DrawTools& tools={});
private:
    friend struct PipelineProbe; // Driver-backed tests inspect attachment lifetimes at idle boundaries.
    using Checkpoint=std::function<void(const tiny2d::RenderTarget2D&)>;
    Checkpoint allocationCheckpoint_; // Optional observer after each successful HDR allocation.
    using Thumbnails=std::array<tiny2d::RenderTarget2D,4>;
    struct Targets {
        tiny2d::RenderTarget2D scene, bright, ping, pong;
        std::uint64_t bytes;
        std::unique_ptr<Thumbnails> thumbnails;
        void allocatePreviews();
        Targets(int width,int height,std::uint64_t total,const Checkpoint&);
    };
    std::unique_ptr<Targets> targets_;
    std::uint64_t generation_=0;
    bool drawable_ = false;
    tiny2d::detail::GLObject triangle_{tiny2d::detail::ObjectKind::VertexArray};
    tiny2d::ShaderProgram bright_, blur_, display_;
    tiny2d::Texture2D white_;
    tiny2d::BatchRenderer batch_{2048,1};
    void sprites(const std::vector<Quad>&, const tiny2d::Camera2D&, Stats&);
};
}
