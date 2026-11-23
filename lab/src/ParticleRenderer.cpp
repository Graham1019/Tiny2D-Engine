#include "ParticleRenderer.h"
#include "Pipeline.h"
#include "ShaderAssets.h"
#include "OpenGL.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <type_traits>

namespace lab {
namespace {
using InstanceGPU=ParticleInstance;
static_assert(std::is_standard_layout_v<InstanceGPU> && std::is_trivially_copyable_v<InstanceGPU>);
static_assert(sizeof(InstanceGPU)==8*sizeof(float) && offsetof(InstanceGPU,r)==4*sizeof(float));

bool valid(const InstanceGPU& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.size) && p.size>0 &&
           p.ageFraction>=0 && p.ageFraction<=1 &&
           std::isfinite(p.r) && p.r>=0 && std::isfinite(p.g) && p.g>=0 &&
           std::isfinite(p.b) && p.b>=0 && std::isfinite(p.emission) && p.emission>=0;
}

// The following full-screen passes assume blending is disabled. On GL 4 use
// draw-buffer zero's independent blend controls, leaving the other lanes alone.
struct AdditiveBlend {
    GLint major, rgb, alpha, srcRGB, dstRGB, srcAlpha, dstAlpha;
    GLboolean enabled;
    void set(GLenum eqR,GLenum eqA,GLenum sr,GLenum dr,GLenum sa,GLenum da) const {
        if (major>=4) {
            glBlendEquationSeparatei(0,eqR,eqA);
            glBlendFuncSeparatei(0,sr,dr,sa,da);
        } else {
            glBlendEquationSeparate(eqR,eqA);
            glBlendFuncSeparate(sr,dr,sa,da);
        }
    }
    AdditiveBlend() {
        glGetIntegerv(GL_MAJOR_VERSION,&major);
        enabled=glIsEnabledi(GL_BLEND,0);
        glGetIntegerv(GL_BLEND_EQUATION_RGB,&rgb); glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&alpha);
        glGetIntegerv(GL_BLEND_SRC_RGB,&srcRGB); glGetIntegerv(GL_BLEND_DST_RGB,&dstRGB);
        glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha); glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
        glEnablei(GL_BLEND,0);
        set(GL_FUNC_ADD,GL_FUNC_ADD,GL_ONE,GL_ONE,GL_ZERO,GL_ONE);
    }
    ~AdditiveBlend() {
        set(rgb,alpha,srcRGB,dstRGB,srcAlpha,dstAlpha);
        if (!enabled) glDisablei(GL_BLEND,0);
    }
};
}
ParticleRenderer::Geometry::Geometry(const Allocate& allocate) {
    struct RestoreBindings {
        GLint vao,buffer;
        ~RestoreBindings() { glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,buffer); }
    } restore{};
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&restore.vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&restore.buffer);
    glBindVertexArray(vao.id());
    glBindBuffer(GL_ARRAY_BUFFER,quad.id());
    constexpr float corners[]={-1,-1, 1,-1, -1,1, 1,1};
    glBufferData(GL_ARRAY_BUFFER,sizeof(corners),corners,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),nullptr);
    glBindBuffer(GL_ARRAY_BUFFER,instances.id());
    constexpr auto bytes=ParticleSimulation::maxCapacity*sizeof(InstanceGPU);
    if (allocate) allocate(bytes);
    else glBufferData(GL_ARRAY_BUFFER,bytes,nullptr,GL_STREAM_DRAW);
    tiny2d::detail::checkGL("particle VBO storage");
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(InstanceGPU),nullptr);
    glVertexAttribDivisor(1,1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,sizeof(InstanceGPU),reinterpret_cast<void*>(offsetof(InstanceGPU,r)));
    glVertexAttribDivisor(2,1);
    tiny2d::detail::checkGL("particle vertex and instance layout");
}
ParticleRenderer::ParticleRenderer() : ParticleRenderer(readShaderAsset("particle.frag"),{}) {}
ParticleRenderer::ParticleRenderer(const std::string& fragment,const Allocate& allocate)
    : geometry_(allocate),program_(readShaderAsset("particle.vert"),fragment,"particle.vert","particle.frag") {}

void ParticleRenderer::draw(const ParticleRenderSnapshot& snapshot,const tiny2d::Camera2D& camera,Stats& stats) {
    geometry_.vao.requireCurrent("particle instancing");
    const auto& instances=snapshot.instances;
    if (instances.empty()) return;
    if (instances.size()>ParticleSimulation::maxCapacity || !std::all_of(instances.begin(),instances.end(),valid))
        throw std::invalid_argument("invalid particle render snapshot");

    const auto bytes=instances.size()*sizeof(InstanceGPU);
    program_.bind();
    program_.setMat4("viewProjection",camera.viewProjectionMatrix());
    glBindVertexArray(geometry_.vao.id());
    glBindBuffer(GL_ARRAY_BUFFER,geometry_.instances.id());
    const auto uploadStart=Clock::now();
    glBufferSubData(GL_ARRAY_BUFFER,0,bytes,instances.data());
    tiny2d::detail::checkGL("particle instance upload");
    stats.instanceUploadUs=micros(uploadStart);
    stats.instanceUploadBytes=bytes;
    stats.activeInstances=static_cast<unsigned>(instances.size());

    AdditiveBlend blend;
    glDrawArraysInstanced(GL_TRIANGLE_STRIP,0,4,static_cast<GLsizei>(instances.size()));
    tiny2d::detail::checkGL("particle instanced draw");
    ++stats.particleDrawCalls;
    ++stats.draws;
}
}
