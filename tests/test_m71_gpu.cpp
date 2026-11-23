#include "M5Test.h"
#include "Pipeline.h"
#include "OpenGL.h"
#include "tiny2d/SceneRenderer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <type_traits>

using namespace tiny2d;
namespace lab {
// Names are inspected solely to verify deletion and identity. They are never
// rebound after a resize. Restore all bindings before handing back the group.
struct PipelineProbe {
    using Names=std::array<std::pair<GLuint,GLuint>,4>;
    static Names inspect(const Pipeline& p,Size expected) {
        GLint draw,texture,unit,vp[4];
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&unit); glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture); glGetIntegerv(GL_VIEWPORT,vp);
        const auto& t=*p.targets_;
        Names names{};
        int i=0;
        for (auto target:{&t.scene,&t.bright,&t.ping,&t.pong}) {
            target->bindDraw(); target->bindColor(0);
            GLint fbo,tex,w,h,format;
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&fbo); glGetIntegerv(GL_TEXTURE_BINDING_2D,&tex);
            glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&w);
            glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&h);
            glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);
            check(w==(i?(expected.width+1)/2:expected.width) && h==(i?(expected.height+1)/2:expected.height)
                && format==GL_RGBA16F && glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,
                "actual attachment layout");
            names[i++]={GLuint(fbo),GLuint(tex)};
        }
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);
        glViewport(vp[0],vp[1],vp[2],vp[3]);
        glBindTexture(GL_TEXTURE_2D,texture); glActiveTexture(unit);
        return names;
    }
};
}
namespace {
GLint get(GLenum key) { GLint value; glGetIntegerv(key,&value); return value; }
std::vector<GLint> snapshot() {
    std::vector<GLint> s;
    for (auto k:{GL_DRAW_FRAMEBUFFER_BINDING,GL_READ_FRAMEBUFFER_BINDING,GL_VERTEX_ARRAY_BINDING,
        GL_CURRENT_PROGRAM,GL_ARRAY_BUFFER_BINDING,GL_PIXEL_UNPACK_BUFFER_BINDING,GL_UNPACK_ROW_LENGTH,
        GL_ACTIVE_TEXTURE,GL_BLEND_SRC_RGB,GL_BLEND_DST_RGB,GL_BLEND_SRC_ALPHA,GL_BLEND_DST_ALPHA,
        GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA,GL_DEPTH_WRITEMASK})
        s.push_back(get(k));
    GLint vp[4]; glGetIntegerv(GL_VIEWPORT,vp); s.insert(s.end(),vp,vp+4);
    GLint polygon[2]; glGetIntegerv(GL_POLYGON_MODE,polygon); s.push_back(polygon[0]);
    for (auto k:{GL_DEPTH_TEST,GL_CULL_FACE,GL_STENCIL_TEST,GL_SCISSOR_TEST,GL_FRAMEBUFFER_SRGB,
        GL_BLEND,GL_RASTERIZER_DISCARD,GL_COLOR_LOGIC_OP}) s.push_back(glIsEnabled(k));
    const auto active=get(GL_ACTIVE_TEXTURE);
    for (int i=0;i<2;++i) {
        glActiveTexture(GL_TEXTURE0+i); s.push_back(get(GL_TEXTURE_BINDING_2D)); s.push_back(get(GL_SAMPLER_BINDING));
        GLboolean mask[4]; glGetBooleani_v(GL_COLOR_WRITEMASK,i,mask);
        s.insert(s.end(),mask,mask+4); s.push_back(glIsEnabledi(GL_BLEND,i));
    }
    glActiveTexture(active);
    return s;
}
using Pixel=std::array<unsigned char,4>;
Pixel sample(int x,int y) {
    const auto saved=get(GL_READ_FRAMEBUFFER_BINDING);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,0); glReadBuffer(GL_BACK);
    Pixel p; glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER,saved);
    return p;
}
int encode(float hdr) {
    float x=hdr/(1+hdr);
    return int(std::round(255*(x<=.0031308f ? 12.92f*x : 1.055f*std::pow(x,1.f/2.4f)-.055f)));
}
void resource() {
    static_assert(!std::is_copy_constructible_v<RenderTarget2D>);
    rejects([]{RenderTarget2D bad("empty",0,3);});
    RenderTarget2D initial("range",7,5), moved(std::move(initial));
    check(initial.width()==0 && initial.height()==0,"moved-from extent");
    rejects([&]{initial.bindDraw();});
    moved.bindDraw();
    constexpr GLfloat hdr[]={4.25f,.18f,1.f,1.f};
    glClearBufferfv(GL_COLOR,0,hdr);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,get(GL_DRAW_FRAMEBUFFER_BINDING));
    GLfloat px[4]; glReadPixels(3,2,1,1,GL_RGBA,GL_FLOAT,px);
    check(std::abs(px[0]-4.25f)<.005f && std::abs(px[1]-.18f)<.002f,"floating point readback");
    glBindFramebuffer(GL_FRAMEBUFFER,0);
}
void exercise(Window& w) {
    lab::Pipeline pipe;
    lab::Picture picture;
    lab::Display display;
    lab::Stats stats;
    const Size odd{101,79};
    picture.worldCamera.setViewportSize(101,79); picture.worldCamera.setPosition(50.5f,39.5f);
    picture.hudCamera=picture.worldCamera;
    picture.world.push_back({{37,24},{26,30},0,{4,.18f,1,1}});
    picture.hud.push_back({{8,8},{12,8},0,{.25f,.5f,.75f,1}});
    detail::GLObject draw(detail::ObjectKind::Framebuffer), read(detail::ObjectKind::Framebuffer),
        vao(detail::ObjectKind::VertexArray), buffer(detail::ObjectKind::Buffer), texture(detail::ObjectKind::Texture);
    ShaderProgram external("#version 330 core\nvoid main(){gl_Position=vec4(0);}",
        "#version 330 core\nout vec4 c;void main(){c=vec4(1);}","caller.vert","caller.frag");
    external.bind(); glBindVertexArray(vao.id());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw.id()); glBindFramebuffer(GL_READ_FRAMEBUFFER,read.id());
    glViewport(2,3,11,13); glEnable(GL_SCISSOR_TEST); glScissor(0,0,1,1);
    glEnable(GL_BLEND); glBlendFunc(GL_DST_ALPHA,GL_SRC_COLOR); glBlendEquation(GL_FUNC_SUBTRACT);
    glColorMask(GL_FALSE,GL_TRUE,GL_FALSE,GL_FALSE); glColorMaski(1,1,0,1,0); glDisablei(GL_BLEND,1);
    glDepthMask(GL_FALSE); glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
    for (auto k:{GL_STENCIL_TEST,GL_DEPTH_TEST,GL_CULL_FACE,GL_FRAMEBUFFER_SRGB,GL_RASTERIZER_DISCARD}) glEnable(k);
    glBindBuffer(GL_ARRAY_BUFFER,buffer.id()); glBindBuffer(GL_PIXEL_UNPACK_BUFFER,buffer.id());
    glPixelStorei(GL_UNPACK_ROW_LENGTH,13);
    GLuint sampler;
    glGenSamplers(1,&sampler);
    for (int i=0;i<2;++i) {
        glActiveTexture(GL_TEXTURE0+i); glBindTexture(GL_TEXTURE_2D,texture.id()); glBindSampler(i,sampler);
    }
    glActiveTexture(GL_TEXTURE4);
    auto saved=snapshot();
    check(pipe.resize(odd) && snapshot()==saved,"resize preserves caller state/PBO");
    auto original=lab::PipelineProbe::inspect(pipe,odd);
    Window other(32,32,"foreign");
    bool wrongResize=false, wrongDraw=false;
    try {pipe.resize(odd);} catch(const std::logic_error&) {wrongResize=true;}
    try {pipe.draw(picture,display,stats);} catch(const std::logic_error&) {wrongDraw=true;}
    w.makeContextCurrent();
    check(wrongResize && wrongDraw && !stats.complete,"wrong context rejected before use");
    auto render=[&](lab::View v,bool bloom,unsigned fullscreen) {
        display.view=v; display.bloom=bloom;
        const auto before=snapshot();
        pipe.draw(picture,display,stats);
        check(stats.complete && stats.fullscreen==fullscreen && stats.passes==2+fullscreen &&
            stats.draws==stats.passes && stats.bytes==2*144,"actual pass counters");
        check(snapshot()==before,"caller state after frame");
    };
    render(lab::View::Final,false,1);
    auto base=sample(50,39), hud=sample(12,67), dark=sample(5,5);
    check(std::abs(base[0]-encode(4))<=3 && std::abs(base[1]-encode(.18f))<=3 &&
        std::abs(base[2]-encode(1))<=3 && std::abs(dark[0]-encode(.002f))<=3,"single display encoding");
    check(std::abs(hud[0]-64)<=2 && std::abs(hud[2]-191)<=2,"HUD display values");
    render(lab::View::Scene,true,1);
    check(base==sample(50,39),"scene equals unbloomed final");
    render(lab::View::Final,true,4);
    auto halo=[&]{int sum=0;for(int x=64;x<70;++x) sum+=sample(x,39)[0];return sum;};
    const auto glow=halo();
    check(glow>6*(dark[0]+4) && dark==sample(5,5) && hud==sample(12,67),"halo/background/HUD regions");
    const auto lit=sample(50,39);
    glDisable(GL_FRAMEBUFFER_SRGB); saved=snapshot();
    render(lab::View::Final,true,4);
    check(lit==sample(50,39) && hud==sample(12,67) && halo()==glow,"caller sRGB independence");
    render(lab::View::Bright,false,2);
    const int brightEdge=halo(); auto bright=sample(50,39);
    check(bright[0]>40 && bright[0]<base[0]-5 && sample(5,5)[0]<3,"bright extraction");
    render(lab::View::Blur,false,4);
    check(halo()>brightEdge+12 && sample(5,5)[0]<3,"blur spreads extracted light");

    picture.hud[0].size.x=-1;
    rejects([&]{pipe.draw(picture,display,stats);});
    check(!stats.complete && stats.passes==5 && stats.draws==5 && snapshot()==saved,"partial HUD failure restores state");
    picture.hud[0].size.x=12; render(lab::View::Final,true,4);
    rejects([&]{pipe.resize({8192,8192});});
    check(lab::PipelineProbe::inspect(pipe,odd)==original && snapshot()==saved,"group survives failed resize");
    check(!pipe.resize({0,0}),"zero suspends"); rejects([&]{pipe.draw(picture,display,stats);});
    check(pipe.resize(odd) && lab::PipelineProbe::inspect(pipe,odd)==original,"same-size resume reuses");
    for (unsigned i=0;i<20;++i) {
        const Size extent=i%2 ? odd : Size{1,1};
        pipe.resize(extent);
        auto fresh=lab::PipelineProbe::inspect(pipe,extent);
        for (const auto& old:original) check(!glIsTexture(old.second) && !glIsFramebuffer(old.first),"retired attachments deleted");
        original=fresh;
        render(static_cast<lab::View>(i%4),false,i%4==1 ? 2 : i%4==2 ? 4 : 1);
        lab::Neon reset(71);
        lab::Picture demo;
        reset.update({});
        reset=lab::Neon(71);
        demo.prepare(reset,display,false,extent,extent,stats);
        pipe.draw(demo,display,stats);
        check(stats.complete && lab::PipelineProbe::inspect(pipe,extent)==fresh,"reset/view reuse attachments");
    }
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glDeleteSamplers(1,&sampler);
    for (auto k:{GL_STENCIL_TEST,GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_RASTERIZER_DISCARD,GL_BLEND}) glDisable(k);
    glColorMask(1,1,1,1); glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0); glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
    // The historical scene renderer still submits to the default framebuffer.
    SceneRenderer legacy;
    legacy.setAsset(0,std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}}));
    Scene scene; scene.find(scene.create())->sprite=SpriteData{};
    legacy.render(scene,picture.worldCamera);
    check(legacy.stats().drawCalls==1 && glGetError()==GL_NO_ERROR,"legacy draw after pipeline");
}
}
int main() { try {
    Window window(128,112,"M7.1 real-context tests");
    resource(); exercise(window);
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; } }
