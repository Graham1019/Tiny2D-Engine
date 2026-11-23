#include "FrameState.h"


namespace lab {
FrameState::FrameState() {
    GLint count,major;
    glGetIntegerv(GL_MAJOR_VERSION,&major);
    indexedBlend_=major>=4;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS,&count);
    buffers_.resize(count);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw_);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read_);
    glGetIntegerv(GL_VIEWPORT,viewport_);
    glGetIntegerv(GL_CURRENT_PROGRAM,&program_);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao_);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&arrayBuffer_);
    glGetIntegerv(GL_ACTIVE_TEXTURE,&active_);
    glGetIntegerv(GL_POLYGON_MODE,polygon_);
    glGetBooleanv(GL_DEPTH_WRITEMASK,&depth_);
    for (unsigned i=0;i<buffers_.size();++i) {
        auto& b=buffers_[i];
        b.blend=glIsEnabledi(GL_BLEND,i);
        glGetBooleani_v(GL_COLOR_WRITEMASK,i,b.color);
        if (indexedBlend_ || i==0) {
            auto read=[&](GLenum parameter,GLint& value) {
                if (indexedBlend_) glGetIntegeri_v(parameter,i,&value);
                else glGetIntegerv(parameter,&value);
            };
            read(GL_BLEND_EQUATION_RGB,b.eqRGB); read(GL_BLEND_EQUATION_ALPHA,b.eqAlpha);
            read(GL_BLEND_SRC_RGB,b.srcRGB); read(GL_BLEND_DST_RGB,b.dstRGB);
            read(GL_BLEND_SRC_ALPHA,b.srcAlpha); read(GL_BLEND_DST_ALPHA,b.dstAlpha);
        }
    }
    for (unsigned i=0;i<std::size(flags);++i) {
        enabled_[i]=glIsEnabled(flags[i]);
        glDisable(flags[i]);
    }
    for (unsigned i=0;i<std::size(textures_);++i) {
        glActiveTexture(GL_TEXTURE0+i);
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&textures_[i]);
        glGetIntegerv(GL_SAMPLER_BINDING,&samplers_[i]);
        glBindSampler(i,0);
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
    glGetIntegerv(GL_DRAW_BUFFER,&backBuffer_);
    glDrawBuffer(GL_BACK);
    glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glDepthMask(GL_FALSE);
}
FrameState::~FrameState() noexcept {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
    glDrawBuffer(backBuffer_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw_);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,read_);
    glViewport(viewport_[0],viewport_[1],viewport_[2],viewport_[3]);
    glUseProgram(program_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER,arrayBuffer_);
    for (unsigned i=0;i<std::size(textures_);++i) {
        glActiveTexture(GL_TEXTURE0+i);
        glBindTexture(GL_TEXTURE_2D,textures_[i]);
        glBindSampler(i,samplers_[i]);
    }
    glActiveTexture(active_);
    for (unsigned i=0;i<std::size(flags);++i)
        if (enabled_[i]) glEnable(flags[i]); else glDisable(flags[i]);
    for (unsigned i=0;i<buffers_.size();++i) {
        const auto& b=buffers_[i];
        if (b.blend) glEnablei(GL_BLEND,i); else glDisablei(GL_BLEND,i);
        glColorMaski(i,b.color[0],b.color[1],b.color[2],b.color[3]);
        if (indexedBlend_) {
            glBlendEquationSeparatei(i,b.eqRGB,b.eqAlpha);
            glBlendFuncSeparatei(i,b.srcRGB,b.dstRGB,b.srcAlpha,b.dstAlpha);
        } else if (i==0) {
            glBlendEquationSeparate(b.eqRGB,b.eqAlpha);
            glBlendFuncSeparate(b.srcRGB,b.dstRGB,b.srcAlpha,b.dstAlpha);
        }
    }
    glDepthMask(depth_);
    glPolygonMode(GL_FRONT_AND_BACK,polygon_[0]);
}
}
