#pragma once
#include "ParticleSimulation.h"
#include "tiny2d/Camera2D.h"
#include "tiny2d/ShaderProgram.h"
#include <functional>

namespace lab {
struct Stats;

// Owned exclusively by Pipeline. draw runs inside its FrameState on the owning context.
class ParticleRenderer {
public:
    ParticleRenderer();
    ParticleRenderer(const ParticleRenderer&)=delete;
    ParticleRenderer& operator=(const ParticleRenderer&)=delete;
    ParticleRenderer(ParticleRenderer&&) noexcept=default;
    ParticleRenderer& operator=(ParticleRenderer&&) noexcept=default;

private:
    friend class Pipeline;
    friend struct PipelineProbe;
    using Allocate=std::function<void(std::size_t)>;
    ParticleRenderer(const std::string& fragment,const Allocate& allocate);
    struct Geometry {
        tiny2d::detail::GLObject vao{tiny2d::detail::ObjectKind::VertexArray};
        tiny2d::detail::GLObject quad{tiny2d::detail::ObjectKind::Buffer};
        tiny2d::detail::GLObject instances{tiny2d::detail::ObjectKind::Buffer};
        explicit Geometry(const Allocate&);
    } geometry_;
    tiny2d::ShaderProgram program_;
    void draw(const ParticleRenderSnapshot&,const tiny2d::Camera2D&,Stats&);
};
}
