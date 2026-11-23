#pragma once
#include "Presentation.h"
#include "tiny2d/BatchRenderer.h"
#include "tiny2d/RenderTarget2D.h"
#include <chrono>

namespace lab {
using Clock = std::chrono::steady_clock;
inline double micros(Clock::time_point start) {
    return std::chrono::duration<double,std::micro>(Clock::now()-start).count();
}
class Pipeline {
public:
    Pipeline();
    ~Pipeline();
    // Called at a frame boundary, before borrowing any attachment. Zero suspends.
    bool resize(tiny2d::Size);
    tiny2d::Size size() const;
    std::uint64_t storage() const;
    void draw(const Picture&, const Display&, Stats& attempt);
private:
    friend struct PipelineProbe; // Driver-backed tests inspect attachment lifetimes at idle boundaries.
    struct Targets {
        tiny2d::RenderTarget2D scene, bright, ping, pong;
        std::uint64_t bytes;
        Targets(int width,int height,std::uint64_t total);
    };
    std::unique_ptr<Targets> targets_;
    bool drawable_ = false;
    tiny2d::detail::GLObject triangle_{tiny2d::detail::ObjectKind::VertexArray};
    tiny2d::ShaderProgram bright_, blur_, display_;
    tiny2d::Texture2D white_;
    tiny2d::BatchRenderer batch_{2048,1};
    void sprites(const std::vector<Quad>&, const tiny2d::Camera2D&, Stats&);
};
}
