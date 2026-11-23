#include "M5Test.h"
#include "Pipeline.h"
#include "OpenGL.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

using namespace tiny2d;
using namespace lab;

namespace lab {
struct PipelineProbe {
    static const RenderTarget2D& scene(const Pipeline& p) { return p.targets_->scene; }
    static const RenderTarget2D& albedo(const Pipeline& p) { return p.targets_->lighting->albedo; }
    static const RenderTarget2D& emission(const Pipeline& p) { return p.targets_->lighting->emission; }
    static const RenderTarget2D& light(const Pipeline& p) { return p.targets_->lighting->light; }
    static std::array<unsigned,3> lightingNames(const Pipeline& p) {
        const auto& group=*p.targets_->lighting;
        return {group.albedo.colorName(),group.emission.colorName(),group.light.colorName()};
    }
    static void checkpoint(Pipeline& p,Pipeline::Checkpoint observe) { p.allocationCheckpoint_=std::move(observe); }
};
}
namespace {
int integer(GLenum key) { GLint v; glGetIntegerv(key,&v); return v; }

template<class T>
std::vector<T> read(unsigned texture,int x,int y,int w,int h,GLenum type) {
    const GLint original=integer(GL_READ_FRAMEBUFFER_BINDING);
    GLuint fbo=0;
    if (texture) {
        glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
        check(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"sampled framebuffer complete");
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    } else {
        glBindFramebuffer(GL_READ_FRAMEBUFFER,0); glReadBuffer(GL_BACK);
    }
    std::vector<T> pixels(4*w*h);
    glReadPixels(x,y,w,h,GL_RGBA,type,pixels.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER,original);
    if (fbo) glDeleteFramebuffers(1,&fbo);
    detail::checkGL("M73 readback");
    return pixels;
}
void rgb(const std::vector<float>& pixel,float r,float g,float b,const char* label) {
    auto near=[](float actual,float expected) {
        return std::abs(actual-expected)<=std::max(.009f,.001f*expected); // RGBA16F rounding.
    };
    check(near(pixel[0],r) && near(pixel[1],g) && near(pixel[2],b),label);
}
int encoded(float hdr) {
    float x=hdr/(1+hdr);
    return int(std::round(255*(x<=.0031308f ? 12.92f*x : 1.055f*std::pow(x,1.f/2.4f)-.055f)));
}
void pixels() {
    Pipeline pipe;
    // Odd physical dimensions, 2x backing scale, a translated and zoomed camera.
    const Size size{123,83};
    pipe.resize(size,true);
    Picture p;
    p.worldCamera.setViewportSize(size.width/2.f,size.height/2.f);
    p.worldCamera.setPosition(9,-7); p.worldCamera.setZoom(1.25f);
    auto worldAt=[&](int x,int y) {
        return p.worldCamera.viewportToWorld({(x+.5f)/2,(size.height-y-.5f)/2});
    };
    p.albedo.push_back({{-100,-100},{200,200},0,{.5f,.5f,.5f,1}});
    const auto emitter=worldAt(51,45);
    p.albedo.push_back({{emitter.x-2,emitter.y-2},{4,4},0,{0,0,0,1}});
    p.emission.push_back({{emitter.x-2,emitter.y-2},{4,4},0,{5,3,2,1}});

    Display d; d.lighting=true; d.ambient=.2f; d.hud=false; d.bloom=false;
    Stats s; auto draw=[&](unsigned previews=0) { pipe.draw(p,d,s,{previews}); check(s.complete,"lit frame"); };
    auto surface=[&](int x,int y) {
        return read<float>(PipelineProbe::scene(pipe).colorName(),x,y,1,1,GL_FLOAT);
    };
    draw();
    const auto ambient=surface(22,70);
    rgb(ambient,.1f,.1f,.1f,"linear albedo times ambient");
    rgb(surface(51,45),5,3,2,"unattenuated HDR emission");
    check(!s.passDraws[World] && s.passDraws[Albedo]==1 && s.passDraws[Emission]==1 &&
          s.passDraws[Light]==1 && s.passDraws[SceneCompose]==1 && !s.passDraws[Preview],"lit pass routing");
    d.ambient=0; draw();
    rgb(surface(22,70),0,0,0,"zero ambient");
    rgb(surface(51,45),5,3,2,"emission independent of ambient");
    d.ambient=.2f;
    Light2D point{worldAt(22,70),6,{1,0,0,1},4};
    p.lights.add(point); draw();
    const auto red=surface(22,70);
    rgb(red,2.1f,.1f,.1f,"red point on gray material preserves HDR");
    const auto offCenter=surface(29,70);
    float q=7.f/(2*1.25f*point.radius);
    rgb(offCenter,.1f+2*(1-q*q)*(1-q*q),.1f,.1f,"world-space radius under zoom and backing scale");
    rgb(surface(40,70),.1f,.1f,.1f,"point contribution vanishes outside radius");
    rgb(surface(90,20),.1f,.1f,.1f,"destination initially ambient");
    check(s.lightUniformBytes==32 && !s.passDraws[Extract],"single light upload; bloom off");

    const auto thumb=pipe.lightPreview();
    d.view=View::Light; draw(16);
    auto thumbnail=[&](int x,int y){return read<unsigned char>(thumb.texture,x,y,1,1,GL_UNSIGNED_BYTE);};
    const auto first=thumbnail(22,70);
    check(std::abs(int(first[0])-encoded(4.2f))<=2 && std::abs(int(first[1])-encoded(.2f))<=2 &&
          first==read<unsigned char>(0,22,70,1,1,GL_UNSIGNED_BYTE),"LightBuffer display and preview single encoding");
    check(s.previewMask==16 && s.passDraws[Preview]==1 && !s.passDraws[Horizontal],"independent Light thumbnail");
    point.position=worldAt(90,20); p.lights.clear(); p.lights.add(point); draw(16);
    const auto former=surface(22,70),destination=surface(90,20);
    rgb(former,.1f,.1f,.1f,"former lit pixel returned to ambient");
    rgb(destination,2.1f,.1f,.1f,"moving light illuminates new surface");
    auto logPixel=[](const char* label,const std::vector<float>& pixel) {
        std::cout<<"gray_scene_linear "<<label<<"=("<<pixel[0]<<','<<pixel[1]<<','<<pixel[2]<<")\n";
    };
    logPixel("ambient",ambient); logPixel("red_at_A",red);
    logPixel("A_after_move",former); logPixel("B_after_move",destination);
    check(thumbnail(90,20)==first && thumbnail(22,70)[0]<first[0] &&
          thumbnail(90,size.height-1-20)[0]<first[0],"current frame thumbnail and GL Y orientation");

    d.view=View::Final; draw();
    const auto light=PipelineProbe::light(pipe).colorName();
    auto lightBefore=read<float>(light,0,0,size.width,size.height,GL_FLOAT);
    auto noBloom=read<unsigned char>(0,57,44,6,3,GL_UNSIGNED_BYTE);
    d.bloom=true; draw();
    auto bloom=read<unsigned char>(0,57,44,6,3,GL_UNSIGNED_BYTE);
    check(int(bloom[0])>int(noBloom[0])+5 && s.passDraws[Horizontal] && s.passDraws[Vertical],"bloom spreads composed emitter into neighboring floor");
    std::cout<<"bloom_neighbor_display_red off="<<int(noBloom[0])<<" on="<<int(bloom[0])
             <<" delta="<<int(bloom[0])-int(noBloom[0])<<'\n';
    check(read<float>(light,0,0,size.width,size.height,GL_FLOAT)==lightBefore,"bloom leaves linear LightBuffer identical");
    p.lights.clear();
    for (unsigned i=0;i<LightList::capacity;++i) p.lights.add(point);
    draw(); rgb(surface(90,20),64.1f,.1f,.1f,"32 overlapping active lights");
    check(s.lightUniformBytes==1024 && s.passDraws[Light]==1,"bounded full-budget upload and single draw");
}
// Read the same interior pixel on the production arena before and after a real
// fixed-step movement. Its footprint stays clear of grid lines and emitters.
void neonFloor() {
    const Size size{320,180};
    constexpr int x=123,y=79;
    Pipeline pipe; pipe.resize(size,true);
    Neon neon(71); Picture picture; Stats stats;
    Display d; d.lighting=true; d.hud=false; d.bloom=false;
    auto sample=[&](bool playerInRange) {
        picture.prepare(neon,d,false,size,size,stats);
        pipe.draw(picture,d,stats);
        const Vector2 world=picture.worldCamera.viewportToWorld({x+.5f,size.height-y-.5f});
        const auto hdr=[&](const RenderTarget2D& target) { return read<float>(target.colorName(),x,y,1,1,GL_FLOAT); };
        const auto albedo=hdr(PipelineProbe::albedo(pipe));
        rgb(albedo,.14f,.16f,.19f,"real Neon floor material");
        rgb(hdr(PipelineProbe::emission(pipe)),0,0,0,"real floor is non-emissive");

        std::array<double,3> sum{d.ambient,d.ambient,d.ambient};
        unsigned contributors=0;
        for (const auto& source:picture.lights) {
            const double distance=std::hypot(double(world.x)-source.position.x,double(world.y)-source.position.y);
            if (distance>=source.radius) continue;
            ++contributors;
            check(source.source.kind==ObjectKind::Player,"floor probe isolates moving player light");
            const double radial=1-std::pow(distance/source.radius,2);
            const double weight=source.intensity*radial*radial;
            sum[0]+=source.color.r*weight;
            sum[1]+=source.color.g*weight;
            sum[2]+=source.color.b*weight;
        }
        check(contributors==unsigned(playerInRange),"player crosses sample influence boundary");
        const auto pixel=hdr(PipelineProbe::scene(pipe));
        for (unsigned c=0;c<3;++c)
            check(std::abs(pixel[c]-albedo[c]*sum[c])<.0015,"production Neon floor follows linear light model");
        std::cout<<"neon_floor tick="<<neon.tick<<" GL_pixel=("<<x<<','<<y<<") world=("<<world.x<<','<<world.y
                 <<") player=("<<neon.player.x<<','<<neon.player.y<<") scene=("<<pixel[0]<<','<<pixel[1]<<','<<pixel[2]
                 <<") expected=("<<albedo[0]*sum[0]<<','<<albedo[1]*sum[1]<<','<<albedo[2]*sum[2]<<")\n";
        return pixel;
    };
    const auto before=sample(true);
    for (int i=0;i<40;++i) neon.update({1,0,false});
    const auto after=sample(false);
    check(before[2]>after[2]+.3f && before[1]>after[1]+.2f,"same Neon floor loses cyan after player moves");
}
struct State {
    int draw,read,active;
    std::array<int,4> viewport;
    std::array<int,3> textures,samplers;
    bool srgb;
};
State state() {
    State s{};
    s.draw=integer(GL_DRAW_FRAMEBUFFER_BINDING); s.read=integer(GL_READ_FRAMEBUFFER_BINDING);
    s.active=integer(GL_ACTIVE_TEXTURE); s.srgb=glIsEnabled(GL_FRAMEBUFFER_SRGB);
    glGetIntegerv(GL_VIEWPORT,s.viewport.data());
    for(unsigned i=0;i<3;++i) {
        glActiveTexture(GL_TEXTURE0+i);
        s.textures[i]=integer(GL_TEXTURE_BINDING_2D); s.samplers[i]=integer(GL_SAMPLER_BINDING);
    }
    glActiveTexture(s.active);
    return s;
}
void unchanged(const State& a) {
    auto b=state();
    check(a.draw==b.draw && a.read==b.read && a.active==b.active && a.srgb==b.srgb &&
          a.viewport==b.viewport && a.textures==b.textures && a.samplers==b.samplers,"caller GL state restored");
}
void lifecycle(Window& window) {
    Pipeline pipe; Display d; Picture pic; Stats stats;
    const Size odd{97,61};
    pipe.resize(odd);
    check(pipe.lightSize().width==0,"no resident light targets initially");
    const auto legacy=pipe.previews();
    const auto generation=pipe.generation(),legacyBytes=pipe.storage();
    const auto scene=PipelineProbe::scene(pipe).colorName();
    rejects([&]{ pipe.lightPreview(); });

    unsigned textures[3],samplers[3],fbos[2];
    glGenTextures(3,textures); glGenSamplers(3,samplers); glGenFramebuffers(2,fbos);
    for (unsigned i=0;i<3;++i) {
        glActiveTexture(GL_TEXTURE0+i); glBindTexture(GL_TEXTURE_2D,textures[i]); glBindSampler(i,samplers[i]);
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fbos[0]); glBindFramebuffer(GL_READ_FRAMEBUFFER,fbos[1]);
    glViewport(7,2,23,11); glEnable(GL_FRAMEBUFFER_SRGB); glActiveTexture(GL_TEXTURE4);
    const State caller=state();
    std::vector<unsigned> partial;
    auto failAt=[&](unsigned n) {
        partial.clear();
        PipelineProbe::checkpoint(pipe,[&,n](const RenderTarget2D& t) {
            partial.push_back(t.colorName());
            if (partial.size()==n) throw std::runtime_error("M73 injected allocation failure");
        });
    };
    failAt(2);
    rejects([&]{pipe.resize(odd,true);}); unchanged(caller);
    check(pipe.storage()==legacyBytes && scene==PipelineProbe::scene(pipe).colorName() &&
          glIsTexture(legacy[0].texture) && partial.size()==2 && !glIsTexture(partial[0]) && !glIsTexture(partial[1]),"first-enable rollback");
    PipelineProbe::checkpoint(pipe,{});
    pipe.resize(odd,true); d.lighting=true;
    auto light=pipe.lightPreview(); auto names=PipelineProbe::lightingNames(pipe);
    check(pipe.generation()==generation && pipe.storage()==legacyBytes+3ull*8*97*61 &&
          pipe.previewBytes()==5ull*4*97*61,"resident HDR and LDR accounting");
    for (int i=0;i<8;++i) {
        d.lighting=i%2; pipe.resize(odd,d.lighting); pipe.draw(pic,d,stats); unchanged(caller);
        check(pipe.storage()==legacyBytes+3ull*8*97*61 && PipelineProbe::lightingNames(pipe)==names &&
              !stats.passDraws[Preview] && stats.passDraws[Light]==unsigned(d.lighting),"toggles reuse and count resident storage");
        if (!d.lighting) rejects([&]{pipe.lightPreview();});
    }
    // Throw after the three textures and compose have been submitted.
    DrawTools fail; fail.gui=[](Stats&) { throw std::runtime_error("M73 injected draw failure"); };
    rejects([&]{pipe.draw(pic,d,stats,fail);}); unchanged(caller);
    check(!stats.complete && stats.passDraws[SceneCompose]==1,"partial stats on late failure");
    failAt(11); // 7 HDR + four LDR. The final Light thumbnail has not been built.
    rejects([&]{pipe.resize({101,67},true);}); unchanged(caller);
    check(partial.size()==11 && std::none_of(partial.begin(),partial.end(),[](auto id){return glIsTexture(id);}) &&
          pipe.generation()==generation && glIsTexture(light.texture) && PipelineProbe::lightingNames(pipe)==names,
          "entire replacement group rolled back, original borrows survive");
    rejects([&]{pipe.lightPreview();});
    PipelineProbe::checkpoint(pipe,{});
    rejects([&]{pipe.resize({integer(GL_MAX_TEXTURE_SIZE)+1,1},true);});
    // The seven HDR attachments fit on their own. Five 256x256 LDRs push the group over budget.
    constexpr int edge=2654;
    static_assert(38ull*edge*edge<=RenderTarget2D::budget &&
                  38ull*edge*edge+5*4*256*256>RenderTarget2D::budget);
    rejects([&]{pipe.resize({edge,edge},true);});
    rejects([&]{pipe.resize({4096,2160},true);});
    rejects([&]{pipe.resize({std::numeric_limits<int>::max(),std::numeric_limits<int>::max()},true);});
    check(!pipe.resize({0,61},true),"zero pauses rendering");
    rejects([&]{pipe.draw(pic,d,stats);});
    pipe.resize(odd,true); pipe.draw(pic,d,stats,{31}); unchanged(caller);
    check(stats.complete && stats.previewMask==31 && pipe.lightPreview().texture==light.texture,"suspended group recovers");
    {
        Window other(20,20,"M73 foreign context");
        rejects([&]{pipe.resize(odd,true);}); rejects([&]{pipe.lightPreview();});
        rejects([&]{pipe.draw(pic,d,stats);});
    }
    window.makeContextCurrent(); unchanged(caller);
    for(int i=0;i<6;++i) {
        const Size next=i%2 ? odd : Size{1,1};
        pipe.resize(next,true);
        for(auto old:names) check(!glIsTexture(old),"previous light attachments deleted");
        check(!glIsTexture(light.texture) && pipe.generation()!=light.generation,"old thumbnail invalidated");
        names=PipelineProbe::lightingNames(pipe); light=pipe.lightPreview();
        check(light.source.width==next.width && light.source.height==next.height &&
              pipe.lightSize().width==next.width && pipe.lightSize().height==next.height,"resized light extent");
        pipe.draw(pic,d,stats,{31}); unchanged(caller);
    }
    pipe.resize({101,69},false);
    check(pipe.lightSize().width==0 && !glIsTexture(light.texture),"resize while disabled retires light targets");
    glBindFramebuffer(GL_FRAMEBUFFER,0); glDeleteFramebuffers(2,fbos);
    glDeleteTextures(3,textures); glDeleteSamplers(3,samplers); glDisable(GL_FRAMEBUFFER_SRGB);
    detail::checkGL("M73 lifecycle");
}
}
int main() { try {
    Window window(160,100,"M7.3 lighting pixel and resource tests");
    pixels(); neonFloor(); lifecycle(window);
} catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; } }
