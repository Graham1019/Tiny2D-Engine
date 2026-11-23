#include "M5Test.h"
#include "Capture.h"
#include "OpenGL.h"
#include "Pipeline.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>

using namespace lab;
using namespace tiny2d;

namespace lab {
struct PipelineProbe {
    static GLuint normal(const Pipeline& p) { return p.targets_->normal->buffer.colorName(); }
    static GLuint scene(const Pipeline& p) { return p.targets_->scene.colorName(); }
    static GLuint light(const Pipeline& p) { return p.targets_->lighting->light.colorName(); }
    static GLuint albedo(const Pipeline& p) { return p.targets_->lighting->albedo.colorName(); }
    static GLuint emission(const Pipeline& p) { return p.targets_->lighting->emission.colorName(); }
    static void checkpoint(Pipeline& p,Pipeline::Checkpoint cb) { p.allocationCheckpoint_=std::move(cb); }
};
}

namespace {
using Pixel=std::array<float,4>;
GLint integer(GLenum parameter) {
    GLint value; glGetIntegerv(parameter,&value); return value;
}
std::vector<Pixel> read(GLuint texture,int x,int y,int width=1,int height=1) {
    const auto saved=integer(GL_READ_FRAMEBUFFER_BINDING);
    GLuint fbo=0;
    if (texture) {
        glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
        check(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"readback FBO incomplete");
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    } else {
        glBindFramebuffer(GL_READ_FRAMEBUFFER,0); glReadBuffer(GL_BACK);
    }
    std::vector<Pixel> values(width*height);
    glReadPixels(x,y,width,height,GL_RGBA,GL_FLOAT,values.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER,saved); if (fbo) glDeleteFramebuffers(1,&fbo);
    detail::checkGL("M74a framebuffer readback");
    return values;
}
void near(double actual,double expected,double tolerance,const char* label) {
    if (!(std::abs(actual-expected)<=tolerance)) {
        std::cerr<<label<<" actual="<<actual<<" expected="<<expected<<" tolerance="<<tolerance<<'\n';
        throw std::runtime_error(label);
    }
}
void rgb(const Pixel& actual,double r,double g,double b,double error,const char* message) {
    near(actual[0],r,error,message); near(actual[1],g,error,message); near(actual[2],b,error,message);
}
void flat(const Pixel& n) { rgb(n,0,0,1,.0001,"flat world normal"); }

void pixels() {
    // One GL pixel spans half a world unit. Pixel centres coincide with tile texel centres.
    const Size size{128,64};
    Pipeline pipe; pipe.resize(size,true,true);
    Picture p; p.worldCamera.setViewportSize(64,32); p.worldCamera.setPosition(0,16);
    p.albedo.push_back({{-32,0},{64,32},0,{.5f,.25f,.125f,1}});
    p.emission.push_back({{-32,0},{64,32},0,{3,2,1,1}});
    Display d; d.lighting=true; d.normalMapping=true; d.hud=false; d.bloom=false; d.ambient=.2f;
    const auto thumb=pipe.normalPreview();
    check(thumb.source.width==128 && thumb.source.height==64 && thumb.thumbnail.width==128 &&
          thumb.thumbnail.height==64 && thumb.generation==pipe.generation(),"Normal preview metadata");
    const auto normals=PipelineProbe::normal(pipe), light=PipelineProbe::light(pipe);
    const auto surface=PipelineProbe::scene(pipe), albedo=PipelineProbe::albedo(pipe);
    const auto emission=PipelineProbe::emission(pipe);
    glBindTexture(GL_TEXTURE_2D,normals);
    GLint format=0; glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);
    glBindTexture(GL_TEXTURE_2D,0);
    check(format==GL_RGBA16F,"world XYZ stored in signed floating point");
    Stats s;
    auto draw=[&](unsigned previews=0) {pipe.draw(p,d,s,{previews}); check(s.complete,"normal frame complete");};
    draw();
    check(s.mask&(1u<<Normal),"Normal pass accounted");
    auto first=read(normals,0,0,128,64);
    auto at=[&](int col,int row) -> const Pixel& {return first[row*128+col];};
    for (const auto& n:first) {
        near(std::hypot(n[0],n[1],n[2]),1,.001,"normal has finite unit length");
        check(n[2]>0,"normal facing observer");
    }
    // World x=2.75 or 29.25 on opposite bevels, y=14.25. Negative x repeats.
    constexpr int x=69,y=35;
    const auto n=at(x,y);
    flat(at(92,35)); check(n[0]<-.5 && at(122,y)[0]>.5,"opposing bevels in world X");
    rgb(at(x-64,y),n[0],n[1],n[2],.001,"periodic in negative world X");
    const auto groove=at(88,19); // (12.25,22.25), versus its reflection at y=9.75.
    check(groove[1]>.4,"asymmetric groove has positive world Y normal"); flat(at(88,44));
    rgb(read(light,x,y)[0],.2,.2,.2,.001,"ambient survives tilted normal");
    rgb(read(emission,x,y)[0],3,2,1,.001,"unmodulated HDR emitter");
    rgb(read(surface,x,y)[0],3.1,2.05,1.025,.003,"HDR ambient plus emission");
    const auto albedoBefore=read(albedo,0,0,128,64);
    const auto emissionBefore=read(emission,0,0,128,64);

    // A double-precision scalar reference consumes the *measured* normal and the
    // world-space source. It shares neither the GLSL nor the CPU material helper.
    auto directional=[&](int sign,float height) {
        const auto world=p.worldCamera.viewportToWorld({(x+.5f)/2,(size.height-y-.5f)/2});
        Light2D point{{world.x+sign*12.f,world.y},48,{1,.5f,.25f,1},8};
        point.height=height; p.lights.clear(); p.lights.add(point); draw();
        const auto measured=read(normals,x,y)[0];
        const double cosine=std::max(0.,(measured[0]*sign*12.+measured[2]*height)/std::hypot(12.,height));
        constexpr double attenuation=(1.-12.*12./(48.*48.))*(1.-12.*12./(48.*48.));
        const double energy=8*attenuation*cosine;
        const auto l=read(light,x,y)[0], c=read(surface,x,y)[0];
        rgb(l,.2+energy,.2+energy/2,.2+energy/4,.007,"Lambert light against independent CPU expectation");
        rgb(c,3+.5*l[0],2+.25*l[1],1+.125*l[2],.006,"linear HDR compose from LightBuffer");
        std::cout<<"bevel world=("<<world.x<<','<<world.y<<") N=("<<measured[0]<<','<<measured[1]
                 <<','<<measured[2]<<") light_dx="<<sign*12<<" height="<<height<<" factor="<<cosine
                 <<" light_red="<<l[0]<<" expected_red="<<.2+energy<<" scene_red="<<c[0]<<'\n';
        return l[0];
    };
    const float left=directional(-1,5), right=directional(1,5);
    check(left>right+3 && left>1,"same texel, distance and intensity changes with incident direction");
    const float high=directional(1,32);
    check(high>right+.5,"source height changes incident direction");

    d.bloom=true; draw();
    check(s.passDraws[Extract] && s.passDraws[Vertical] && read(normals,0,0,128,64)==first,"Bloom preserves signed normals");
    d.normalMaterial.strength=2; draw();
    const auto strong=read(normals,x,y)[0]; const auto len=std::hypot(2.*n[0],2.*n[1],double(n[2]));
    rgb(strong,2*n[0]/len,2*n[1]/len,n[2]/len,.002,"strength two rescales XY then renormalizes");
    d.normalMaterial.strength=0; draw(); flat(read(normals,x,y)[0]);
    d.normalMaterial.strength=1; d.normalMaterial.kind=NormalMaterialKind::Flat; draw(); flat(read(normals,x,y)[0]);
    check(read(emission,0,0,128,64)==emissionBefore && read(albedo,0,0,128,64)==albedoBefore,
          "strength and material changes preserve exact emission and albedo bytes");

    // Direct data encoding on the diagnostic screen and on the borrowed thumbnail.
    d.view=View::Normal; d.normalMaterial.kind=NormalMaterialKind::ProceduralTile;
    constexpr auto normalBit=1u<<unsigned(View::Normal);
    draw(normalBit);
    for (auto coords:{std::array<int,2>{88,19},{88,44},{69,35},{122,35}}) {
        const auto a=coords[0], b=coords[1]; const auto& v=at(a,b);
        const auto preview=read(thumb.texture,a,b)[0], screen=read(0,a,b)[0];
        rgb(preview,.5*(v[0]+1),.5*(v[1]+1),.5*(v[2]+1),1.2/255,"Normal thumbnail vector mapping");
        rgb(screen,preview[0],preview[1],preview[2],1.2/255,"Normal view vector mapping");
    }
    check(s.passDraws[Preview]==1 && s.previewMask==normalBit,"direct single preview pass");
    d.normalMaterial.strength=0; draw(normalBit);
    rgb(read(thumb.texture,88,19)[0],.5,.5,1,1./255,"current frame overwrites former asymmetric marker");

    // Move and zoom the camera, then change to an odd framebuffer. Centre pixel
    // samples exactly the original bevel texel at each backing scale.
    d.normalMaterial.strength=1;
    for (Size next:{Size{101,77},Size{203,155},Size{1,1}}) {
        pipe.resize(next,true,true);
        p.worldCamera.setViewportSize(101,77); p.worldCamera.setPosition(2.75f,14.25f); p.worldCamera.setZoom(1.75f);
        draw(); const auto sampled=read(PipelineProbe::normal(pipe),next.width/2,next.height/2)[0];
        rgb(sampled,n[0],n[1],n[2],.002,"same world texel after resize, translation and zoom");
        rgb(read(PipelineProbe::albedo(pipe),next.width/2,next.height/2)[0],.5,.25,.125,.001,"same underlying geometry");
    }
    p.worldCamera.setPosition(335,155); draw(); flat(read(PipelineProbe::normal(pipe),0,0)[0]);
}

struct GlState {
    std::array<GLint,4> viewport;
    std::array<GLint,3> textures,samplers;
    GLint active,read,draw,unpack,rowLength,skipRows,skipPixels;
    GLboolean srgb,blend;
};
GlState state() {
    GlState s{};
    s.active=integer(GL_ACTIVE_TEXTURE); s.read=integer(GL_READ_FRAMEBUFFER_BINDING); s.draw=integer(GL_DRAW_FRAMEBUFFER_BINDING);
    s.srgb=glIsEnabled(GL_FRAMEBUFFER_SRGB); s.blend=glIsEnabledi(GL_BLEND,0);
    s.unpack=integer(GL_PIXEL_UNPACK_BUFFER_BINDING); s.rowLength=integer(GL_UNPACK_ROW_LENGTH);
    s.skipRows=integer(GL_UNPACK_SKIP_ROWS); s.skipPixels=integer(GL_UNPACK_SKIP_PIXELS);
    glGetIntegerv(GL_VIEWPORT,s.viewport.data());
    for (unsigned i=0;i<3;++i) {
        glActiveTexture(GL_TEXTURE0+i); s.textures[i]=integer(GL_TEXTURE_BINDING_2D); s.samplers[i]=integer(GL_SAMPLER_BINDING);
    }
    glActiveTexture(s.active); return s;
}
void restored(const GlState& saved) {
    const auto s=state();
    check(s.viewport==saved.viewport && s.textures==saved.textures && s.samplers==saved.samplers &&
          s.active==saved.active && s.read==saved.read && s.draw==saved.draw && s.srgb==saved.srgb &&
          s.blend==saved.blend && s.unpack==saved.unpack && s.rowLength==saved.rowLength &&
          s.skipRows==saved.skipRows && s.skipPixels==saved.skipPixels,"caller GL state across normal frame or failure");
}
void resources(Window& window) {
    Pipeline pipe; Picture p; Display d; Stats s; d.lighting=true;
    const Size size{99,73}; pipe.resize(size,true);
    const auto oldScene=PipelineProbe::scene(pipe),oldLight=PipelineProbe::light(pipe);
    const auto legacyBytes=pipe.storage();
    pipe.draw(p,d,s); check(!(s.mask&(1u<<Normal)) && !pipe.normalSize().width,"radial path has no normal pass or target");
    rejects([&]{pipe.normalPreview();});

    GLuint tex[3],samplers[3],fbos[2],pbo=0;
    glGenTextures(3,tex); glGenSamplers(3,samplers); glGenFramebuffers(2,fbos); glGenBuffers(1,&pbo);
    for (unsigned i=0;i<3;++i) {
        glActiveTexture(GL_TEXTURE0+i); glBindTexture(GL_TEXTURE_2D,tex[i]); glBindSampler(i,samplers[i]);
    }
    glActiveTexture(GL_TEXTURE4); glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,9); glPixelStorei(GL_UNPACK_SKIP_ROWS,3); glPixelStorei(GL_UNPACK_SKIP_PIXELS,2);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fbos[0]); glBindFramebuffer(GL_READ_FRAMEBUFFER,fbos[1]);
    glViewport(3,5,13,17); glEnable(GL_FRAMEBUFFER_SRGB); glEnablei(GL_BLEND,0);
    const auto external=state();
    const ImageData badTile{64,64,{128,128,255,255}};
    rejects([&]{ Texture2D incomplete(badTile,TextureFilter::Linear,TextureWrap::Repeat); });
    const ImageData validTexel{1,1,{128,128,255,255}};
    rejects([&]{ Texture2D invalidWrap(validTexel,TextureFilter::Linear,static_cast<TextureWrap>(-1)); });
    restored(external);
    std::vector<GLuint> allocated;
    PipelineProbe::checkpoint(pipe,[&](const auto& t) {
        allocated.push_back(t.colorName()); throw std::runtime_error("test: incomplete normal enable");
    });
    rejects([&]{pipe.resize(size,true,true);}); restored(external);
    check(allocated.size()==1 && !glIsTexture(allocated.front()) && pipe.storage()==legacyBytes &&
          PipelineProbe::scene(pipe)==oldScene && PipelineProbe::light(pipe)==oldLight,"failed normal enable rolled back");

    PipelineProbe::checkpoint(pipe,{});
    pipe.resize(size,true,true); restored(external);
    const auto normal=PipelineProbe::normal(pipe);
    const auto bytes=pipe.storage();
    check(bytes==legacyBytes+8ull*size.width*size.height && pipe.materialBytes()==4ull*64*64,"normal and tile storage accounting");
    pipe.previews(); pipe.lightPreview(); const auto thumb=pipe.normalPreview();
    const auto generation=pipe.generation();
    check(pipe.previewBytes()==6ull*size.width*size.height*4,"six resident RGBA8 previews");
    d.normalMapping=true; pipe.draw(p,d,s,{63}); restored(external);
    check(s.previewMask==63 && s.passDraws[Preview]==6 && s.passDraws[Normal]==1,"six current thumbnails, one Normal draw");
    DrawTools failure; failure.previews=63; failure.gui=[](Stats&){throw std::runtime_error("test: late draw failure");};
    rejects([&]{pipe.draw(p,d,s,failure);}); restored(external);
    check(!s.complete && s.passDraws[Normal]==1,"partial normal frame accounting");

    allocated.clear();
    PipelineProbe::checkpoint(pipe,[&](const RenderTarget2D& t) {
        allocated.push_back(t.colorName()); if (allocated.size()==13) throw std::runtime_error("test: partial full replacement");
    });
    rejects([&]{pipe.resize({121,89},true,true);}); restored(external);
    check(allocated.size()==13 && std::none_of(allocated.begin(),allocated.end(),[](GLuint id){return glIsTexture(id);}) &&
          glIsTexture(normal) && glIsTexture(thumb.texture) && pipe.generation()==generation,"resize replacement rollback");
    rejects([&]{pipe.normalPreview();});
    PipelineProbe::checkpoint(pipe,{});
    constexpr int edge=2410; // RGBA16F group alone fits, six thumbnails exceed budget.
    static_assert(46ull*edge*edge+64*64*4<RenderTarget2D::budget &&
                  46ull*edge*edge+6*256*256*4+64*64*4>RenderTarget2D::budget);
    rejects([&]{pipe.resize({edge,edge},true,true);});
    check(!pipe.resize({0,73},true,true),"zero extent suspends");
    rejects([&]{pipe.draw(p,d,s);}); rejects([&]{pipe.normalPreview();});
    pipe.resize(size,true,true); pipe.draw(p,d,s,{63}); restored(external);
    check(pipe.normalPreview().texture==thumb.texture && s.complete,"same-size resume after budget and injected failures");
    {
        Window other(20,20,"foreign normal context");
        rejects([&]{pipe.resize(size,true,true);}); rejects([&]{pipe.draw(p,d,s);}); rejects([&]{pipe.normalPreview();});
    }
    window.makeContextCurrent(); restored(external);
    for (int mode:{0,1,2}) {
        d.lighting=mode>0; d.normalMapping=mode==2; pipe.resize(size,d.lighting,d.normalMapping);
        pipe.draw(p,d,s); restored(external);
        check(s.passDraws[Normal]==unsigned(mode==2) && s.passDraws[Light]==unsigned(mode>0) &&
              pipe.storage()==bytes && PipelineProbe::normal(pipe)==normal,"retained targets, exact three-way pass routing");
        if (mode<2) rejects([&]{pipe.normalPreview();});
    }
    pipe.resize({101,75},true,false);
    check(!pipe.normalSize().width && !glIsTexture(normal) && !glIsTexture(thumb.texture),"disabled resize retires old borrows");
    glBindFramebuffer(GL_FRAMEBUFFER,0); glDeleteFramebuffers(2,fbos);
    glDeleteSamplers(3,samplers); glDeleteTextures(3,tex); glDeleteBuffers(1,&pbo);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,0); glPixelStorei(GL_UNPACK_SKIP_ROWS,0); glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
    glDisable(GL_FRAMEBUFFER_SRGB); glDisable(GL_BLEND); glActiveTexture(GL_TEXTURE0);
    detail::checkGL("normal lifecycle");
}

// The two captures use the same camera, material, tick and seed. At tick 16 the
// initial bolt/burst and all relays are identical; only the player's held motion differs.
void neonPair(Window& window,const std::filesystem::path& directory) {
    const auto pixels=window.framebufferSize(), logical=window.size();
    Pipeline pipe; pipe.resize(pixels,true,true); Stats s; Display d;
    d.normalMapping=true; d.lighting=true; d.hud=false;
    Picture picture; std::array<Pixel,2> result;
    for (int index=0;index<2;++index) {
        Neon neon(71);
        for (int tick=0;tick<16;++tick) neon.update({index?1:-1,0,false});
        picture.prepare(neon,d,false,logical,pixels,s);
        picture.worldCamera.setPosition(-61.25f,12.25f);
        picture.worldCamera.setZoom(logical.width/180.f);
        pipe.draw(picture,d,s);
        const int cx=pixels.width/2,cy=pixels.height/2;
        result[index]=read(PipelineProbe::scene(pipe),cx,cy)[0];
        const auto n=read(PipelineProbe::normal(pipe),cx,cy)[0];
        std::cout<<"neon_same_floor side="<<(index?"right":"left")<<" tick="<<neon.tick
                 <<" player=("<<neon.player.x<<','<<neon.player.y<<") pixel=("<<cx<<','<<cy
                 <<") N=("<<n[0]<<','<<n[1]<<','<<n[2]<<") scene_blue="<<result[index][2]<<'\n';
        if (!directory.empty())
            capture((directory/(index?"direction-right.png":"direction-left.png")).string(),pixels);
    }
    check(result[0][2]>result[1][2]+.12f,"real player movement changes fixed bevel brightness");
}
}
int main(int argc,char** argv) {
    try {
        Window window(argc==2?800:160,argc==2?450:100,"M7.4a normal materials");
        pixels(); resources(window); neonPair(window,argc==2?argv[1]:"");
    } catch (const std::exception& e) {std::cerr<<e.what()<<'\n'; return 1;}
}
