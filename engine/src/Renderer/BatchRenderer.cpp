#include "tiny2d/BatchRenderer.h"
#include "OpenGL.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <string>
#include <array>
namespace tiny2d {
namespace {
struct Vertex { float x,y,u,v,r,g,b,a,slot; };
constexpr auto vertex=R"(#version 330 core
layout(location=0) in vec2 position;
layout(location=1) in vec2 uv;
layout(location=2) in vec4 tint;
layout(location=3) in float slot;
uniform mat4 uViewProjection;
out vec2 vUV; out vec4 vTint; flat out int vSlot;
void main(){gl_Position=uViewProjection*vec4(position,0,1);vUV=uv;vTint=tint;vSlot=int(slot);}
)";
std::string fragment(unsigned n) {
    std::string s="#version 330 core\nin vec2 vUV; in vec4 vTint; flat in int vSlot; out vec4 color;\nuniform sampler2D uTextures["+std::to_string(n)+"];\nvoid main(){vec2 dx=dFdx(vUV),dy=dFdy(vUV);vec4 t=vec4(0);switch(vSlot){";
    for(unsigned i=0;i<n;++i) s+="case "+std::to_string(i)+":t=textureGrad(uTextures["+std::to_string(i)+"],vUV,dx,dy);break;";
    return s+="}color=t*vTint;}";
}
unsigned slots(unsigned requested) {
    detail::requireContext("batch construction");
    GLint n=0;glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,&n);detail::checkGL("texture limits");
    if(!requested||requested>8||n<1) throw std::invalid_argument("texture slots must be 1..8");
    return std::min(requested,static_cast<unsigned>(n));
}
std::size_t capacity(std::size_t n) {
    if(!n||n>std::numeric_limits<GLuint>::max()/4||n>std::size_t(std::numeric_limits<GLsizei>::max())/6||n>std::size_t(std::numeric_limits<GLsizeiptr>::max())/(4*sizeof(Vertex))) throw std::invalid_argument("batch capacity overflow");
    return n;
}
}
struct BatchRenderer::Impl {
    std::size_t cap; unsigned limit;
    ShaderProgram program;
    detail::GLObject vbo{detail::ObjectKind::Buffer},ebo{detail::ObjectKind::Buffer},vao{detail::ObjectKind::VertexArray};
    std::vector<Vertex> vertices; std::vector<GLuint> textures;
    RenderStats stats; bool active=false;
    GLint oldProgram=0,oldVAO=0,oldVBO=0,oldUnit=0,srcRGB=0,dstRGB=0,srcA=0,dstA=0,eqRGB=0,eqA=0;
    GLboolean blend=false; std::array<GLint,8> bindings{};
    Impl(std::size_t n,unsigned t):cap(capacity(n)),limit(slots(t)),program(vertex,fragment(limit),"batch.vert","batch.frag") {
        vertices.reserve(cap*4);textures.reserve(limit);
        std::vector<GLuint> indices(cap*6);
        for(std::size_t i=0;i<cap;++i) {const GLuint b=static_cast<GLuint>(i*4); const GLuint q[]={b,b+1,b+2,b,b+2,b+3};std::copy(q,q+6,indices.begin()+i*6);}
        GLint va=0,buf=0;glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&va);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buf);
        struct Restore {GLint va,buf;~Restore(){glBindVertexArray(va);glBindBuffer(GL_ARRAY_BUFFER,buf);}} restore{va,buf};
        glBindVertexArray(vao.id());glBindBuffer(GL_ARRAY_BUFFER,vbo.id());glBufferData(GL_ARRAY_BUFFER,cap*4*sizeof(Vertex),nullptr,GL_STREAM_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo.id());glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*sizeof(GLuint),indices.data(),GL_STATIC_DRAW);
        const GLint sizes[]={2,2,4,1};const std::size_t offsets[]={0,2,4,8};
        for(unsigned i=0;i<4;++i){glEnableVertexAttribArray(i);glVertexAttribPointer(i,sizes[i],GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsets[i]*sizeof(float)));}
        detail::checkGL("batch allocation");
    }
    void restore() noexcept {
        if(!active)return;
        for(unsigned i=0;i<limit;++i){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,bindings[i]);}
        glActiveTexture(oldUnit);glUseProgram(oldProgram);glBindVertexArray(oldVAO);glBindBuffer(GL_ARRAY_BUFFER,oldVBO);
        glBlendFuncSeparate(srcRGB,dstRGB,srcA,dstA);glBlendEquationSeparate(eqRGB,eqA);if(blend)glEnable(GL_BLEND);else glDisable(GL_BLEND);
        active=false;
    }
    ~Impl(){restore();}
};
BatchRenderer::BatchRenderer(std::size_t n,unsigned t):impl_(std::make_unique<Impl>(n,t)){}
BatchRenderer::~BatchRenderer()=default;
BatchRenderer::BatchRenderer(BatchRenderer&&) noexcept=default;
BatchRenderer& BatchRenderer::operator=(BatchRenderer&& other) {
    if(this==&other) return *this;
    if((impl_ && impl_->active) || (other.impl_ && other.impl_->active))
        throw std::logic_error("batch move assignment requires inactive source and target");
    // Validate before deleting the target or taking ownership of the source.
    if(impl_) impl_->vao.requireCurrent("batch move assignment target");
    if(other.impl_) other.impl_->vao.requireCurrent("batch move assignment source");
    impl_=std::move(other.impl_);
    return *this;
}
BatchRenderer::Impl& BatchRenderer::get()const{if(!impl_)throw std::logic_error("moved-from batch");return *impl_;}
const RenderStats& BatchRenderer::stats()const{return get().stats;}
unsigned BatchRenderer::textureSlots()const{return get().limit;}
void BatchRenderer::beginFrame(const Camera2D& camera){
    auto& b=get();b.vao.requireCurrent("batch begin");if(b.active)throw std::logic_error("nested begin");
    const auto matrix=camera.viewProjectionMatrix();
    glGetIntegerv(GL_CURRENT_PROGRAM,&b.oldProgram);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&b.oldVAO);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&b.oldVBO);glGetIntegerv(GL_ACTIVE_TEXTURE,&b.oldUnit);
    glGetIntegerv(GL_BLEND_SRC_RGB,&b.srcRGB);glGetIntegerv(GL_BLEND_DST_RGB,&b.dstRGB);glGetIntegerv(GL_BLEND_SRC_ALPHA,&b.srcA);glGetIntegerv(GL_BLEND_DST_ALPHA,&b.dstA);glGetIntegerv(GL_BLEND_EQUATION_RGB,&b.eqRGB);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&b.eqA);b.blend=glIsEnabled(GL_BLEND);
    for(unsigned i=0;i<b.limit;++i){glActiveTexture(GL_TEXTURE0+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&b.bindings[i]);}
    b.active=true;b.stats={};
    try{b.program.bind();b.program.setMat4("uViewProjection",matrix);
    for(unsigned i=0;i<b.limit;++i)b.program.setInt(("uTextures["+std::to_string(i)+"]").c_str(),i);
    glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    }catch(...){b.restore();throw;}
}
void BatchRenderer::draw(const Texture2D& texture,Vector2 p,Vector2 s,float angle,Color c){
    auto& b=get();if(!b.active)throw std::logic_error("draw outside frame");
    b.vao.requireCurrent("batch draw");
    texture.requireCurrentContext(); // Must precede capacity/slot flush and all queue mutation.
    if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(s.x)||!std::isfinite(s.y)||s.x<=0||s.y<=0||!std::isfinite(angle)||!std::isfinite(c.r)||!std::isfinite(c.g)||!std::isfinite(c.b)||!std::isfinite(c.a))throw std::invalid_argument("invalid sprite");
    Vertex quad[4];const float uv[4][2]={{0,0},{1,0},{1,1},{0,1}};
    const auto model=Mat4::translation(p.x+s.x/2,p.y+s.y/2)*Mat4::rotationZ(angle)*Mat4::scale(s.x,s.y)*Mat4::translation(-.5f,-.5f);
    for(unsigned i=0;i<4;++i){float u=uv[i][0],v=uv[i][1];float x=model.values[0]*u+model.values[4]*v+model.values[12],y=model.values[1]*u+model.values[5]*v+model.values[13];if(!std::isfinite(x)||!std::isfinite(y))throw std::invalid_argument("transform overflow");quad[i]={x,y,u,v,c.r,c.g,c.b,c.a,0};}
    if(b.vertices.size()/4==b.cap)flush();
    auto it=std::find(b.textures.begin(),b.textures.end(),texture.nativeHandle());
    if(it==b.textures.end()&&b.textures.size()==b.limit){flush();it=b.textures.end();}
    unsigned slot=static_cast<unsigned>(it-b.textures.begin());
    if(it==b.textures.end())b.textures.push_back(texture.nativeHandle());
    for(auto& v:quad){v.slot=float(slot);b.vertices.push_back(v);}++b.stats.submittedSprites;
}
void BatchRenderer::flush(){
    auto& b=get();if(!b.active)throw std::logic_error("flush outside frame");b.vao.requireCurrent("batch flush");if(b.vertices.empty())return;
    glBindVertexArray(b.vao.id());glBindBuffer(GL_ARRAY_BUFFER,b.vbo.id());
    glBufferData(GL_ARRAY_BUFFER,b.cap*4*sizeof(Vertex),nullptr,GL_STREAM_DRAW); // Orphan: driver retains in-flight storage.
    glBufferSubData(GL_ARRAY_BUFFER,0,b.vertices.size()*sizeof(Vertex),b.vertices.data());detail::checkGL("batch upload");
    b.stats.uploadedVertexBytes+=b.vertices.size()*sizeof(Vertex);
    for(unsigned i=0;i<b.textures.size();++i){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,b.textures[i]);++b.stats.textureBinds;}
    // Every active sampler must reference a complete texture, including unused branches.
    for(unsigned i=0;i<b.limit;++i)b.program.setInt(("uTextures["+std::to_string(i)+"]").c_str(),i<b.textures.size()?i:0);
    glDrawElements(GL_TRIANGLES,static_cast<GLsizei>(b.vertices.size()/4*6),GL_UNSIGNED_INT,nullptr);detail::checkGL("batch draw");
    ++b.stats.drawCalls;++b.stats.batchFlushes;b.vertices.clear();b.textures.clear();
}
void BatchRenderer::endFrame(){auto& b=get();if(!b.active)throw std::logic_error("end outside frame");try{flush();}catch(...){b.vertices.clear();b.textures.clear();b.restore();throw;}b.restore();}
}
