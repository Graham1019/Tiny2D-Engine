#include "M5Test.h"
#include "Capture.h"
#include "Occlusion.h"
#include "OpenGL.h"
#include "Pipeline.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numeric>

using namespace lab;
using namespace tiny2d;

namespace lab {
struct PipelineProbe {
    static GLuint mask(const Pipeline& p) { return p.targets_->shadow->buffer.colorName(); }
    static GLuint normal(const Pipeline& p) { return p.targets_->normal->buffer.colorName(); }
    static GLuint light(const Pipeline& p) { return p.targets_->lighting->light.colorName(); }
    static GLuint scene(const Pipeline& p) { return p.targets_->scene.colorName(); }
    static GLuint bright(const Pipeline& p) { return p.targets_->bright.colorName(); }
    static void bufferAllocator(Pipeline& p,std::function<void(std::size_t)> allocate) {
        p.allocateShadowBuffer_=std::move(allocate);
    }
    static void checkpoint(Pipeline& p, Pipeline::Checkpoint cb) { p.allocationCheckpoint_=std::move(cb); }
};
}

namespace {
using Pixel=std::array<float,4>;
using Image=std::vector<Pixel>;
GLint integer(GLenum property) {
    GLint value; glGetIntegerv(property,&value); return value;
}
Image read(GLuint texture,Size size) {
    const GLint previous=integer(GL_READ_FRAMEBUFFER_BINDING);
    GLuint fbo=0;
    glBindFramebuffer(GL_READ_FRAMEBUFFER,0);
    if (texture) {
        glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
        check(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"fixture readback FBO");
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    } else glReadBuffer(GL_BACK);
    Image data(std::size_t(size.width)*size.height);
    glReadPixels(0,0,size.width,size.height,GL_RGBA,GL_FLOAT,data.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER,previous);
    if (fbo) glDeleteFramebuffers(1,&fbo);
    detail::checkGL("lighting fixture readback");
    return data;
}
void near(float observed,float expected,float tolerance,const char* label) {
    if (!(std::abs(observed-expected)<=tolerance)) {
        std::cerr<<label<<" observed="<<observed<<" expected="<<expected<<" tolerance="<<tolerance<<'\n';
        throw std::runtime_error(label);
    }
}
constexpr Size extent{129,97};
constexpr LabHandle player{7,ObjectKind::Player,0,1}, relay{7,ObjectKind::Relay,0,1};
std::size_t at(int x,int y) { return (48-y)*extent.width+64+x; }
Light2D redLight(float x) { return {{x,0},54,{1,0,0,1},3,player,10}; }
Light2D blueLight() { return {{26,6},48,{0,0,1,1},4,relay,14}; }

void referenceMask(const Light2D& light,const Picture& picture,const Image& mask) {
    int blocked=0,visible=0;
    for (int y=-44;y<=44;y+=3) for (int x=-60;x<=60;x+=3) {
        if (std::hypot(x-light.position.x,y-light.position.y)>light.radius-1) continue;
        const Vector2 receiver{float(x),float(y)};
        const bool behind=isOccluded(light.position,receiver,picture.occluders);
        // Exclude pixels touched by a boundary. Centre and four surrounding
        // corners must give the same answer independently of projected triangles.
        bool interior=true;
        for (float dx:{-.65f,.65f}) for (float dy:{-.65f,.65f})
            interior &= behind==isOccluded(light.position,{x+dx,y+dy},picture.occluders);
        if (!interior) continue;
        near(mask[at(x,y)][0],behind?0.f:1.f,0,"CPU ray / GPU mask");
        if (behind) ++blocked; else ++visible;
    }
    check(blocked>30 && visible>30,"ray oracle compared interior samples on both sides");
    std::cout<<"source=("<<light.position.x<<','<<light.position.y<<") CPU/GPU blocked="<<blocked
             <<" visible="<<visible<<'\n';
}
void pixels() {
    Pipeline pipe;
    Display d; d.hud=false; d.bloom=false; d.lighting=true; d.shadows=true; d.ambient=.25f;
    Picture pic; pic.worldCamera.setViewportSize(extent.width,extent.height);
    pic.albedo.push_back({{-65,-49},{131,99},0,{.5f,.5f,.5f,1}});
    pic.emission.push_back({{-65,-49},{131,99},0,{6,0,0,1}});
    pic.occluders.addRect(1,{-1,-9},{3,25});
    pic.lights.add(redLight(-20));
    Stats s;
    DrawTools tools; tools.shadowSource=player; tools.frame=90; tools.tick=84; tools.epoch=7;
    auto draw=[&] {
        pipe.resize(extent,d.lighting,d.normalMapping,d.shadows);
        pipe.draw(pic,d,s,tools);
        check(s.complete && s.draws==std::accumulate(s.passDraws.begin(),s.passDraws.end(),0u),"draw sum");
    };
    auto lightImage=[&]{return read(PipelineProbe::light(pipe),extent);};
    auto maskImage=[&]{return read(PipelineProbe::mask(pipe),extent);};
    draw();
    const auto first=lightImage();
    check(first[at(-10,3)][0]>2,"front side receives red");
    near(first[at(12,3)][0],.25f,.001f,"red shadow retains ambient");
    near(first[at(62,-39)][0],.25f,.001f,"outside radius retains ambient");
    near(read(PipelineProbe::scene(pipe),extent)[at(12,3)][0],6.125f,.002f,"emission under red shadow");
    check(!s.inspectRebuilds && s.maskRebuilds==1,"no idle inspection");
    pic.lights.add(blueLight());
    d.view=View::Shadow;
    const auto thumb=pipe.shadowPreview();
    tools.previews=1u<<unsigned(View::Shadow);
    draw();
    auto mixed=lightImage();
    const Pixel target=mixed[at(12,3)];
    near(target[0],.25f,.001f,"target red is ambient");
    near(target[1],.25f,.001f,"target green is ambient");
    near(target[2],.25f+4*attenuation(blueLight(),{12,3}),.005f,"visible blue remains");
    near(target[3],1,0,"HDR destination alpha");
    check(s.maskRebuilds==2 && s.lightDraws==2 && s.inspectRebuilds==1 && s.activeCasters==2,
          "two independent direct-light draws and one inspection");
    check(s.shadowMask && s.shadowMask->source==player && s.shadowMask->frame==90 && s.shadowMask->tick==84 &&
          s.shadowMask->epoch==7 && s.shadowMask->generation==pipe.generation(),"current inspection identity");
    // Add a second, overlapping shadow only after verifying the single-wall RGB fixture.
    pic.occluders.addSegment(2,{5,11},{11,22});
    const auto walls=pic.occluders;
    draw();
    const auto red=maskImage();
    referenceMask(redLight(-20),pic,red);
    for (const auto& img:{read(thumb.texture,extent),read(0,extent)}) {
        for (std::size_t i=0;i<img.size();++i) near(img[i][0],red[i][0],1.f/255,"display orientation and linear grayscale");
    }
    std::cout<<"two-light target world=(12,3) red="<<target[0]<<" green="<<target[1]<<" blue="<<target[2]
             <<" alpha="<<target[3]<<'\n';
    tools.shadowSource=relay; ++tools.frame; draw();
    const auto blue=maskImage(); referenceMask(blueLight(),pic,blue);
    check(red[at(12,3)][0]==0 && blue[at(12,3)][0]==1 && s.shadowMask->source==relay &&
          s.shadowMask->frame==91,"selected caster changed on this frame");
    tools.shadowSource=player; d.shadowLights=1; draw();
    check(s.skippedCasters==1 && s.activeCasters==1 && s.lightDraws==1,"one overflow light routed to base");
    near(lightImage()[at(12,3)][2],target[2],.005f,"overflow blue continues to illuminate");
    tools.shadowSource=LabHandle{6,ObjectKind::Player,0,1}; tools.previews=0; draw();
    check(!s.shadowMask && !s.inspectRebuilds,"expired epoch has no current mask");
    tools.shadowSource=player; pic.lights.clear(); pic.lights.add(redLight(20)); draw();
    const auto moved=maskImage(); referenceMask(redLight(20),pic,moved);
    check(red[at(-10,3)][0]==1 && moved[at(-10,3)][0]==0 && moved[at(12,3)][0]==1,"shadow flips with light");
    draw(); check(maskImage()==moved,"identical light and walls reproduce mask");

    pic.lights.clear(); pic.lights.add(redLight(-20)); pic.lights.add(blueLight()); d.shadowLights=4;
    d.normalMapping=true; d.view=View::Final; tools.previews=1u<<unsigned(View::Shadow); draw();
    auto normal=read(PipelineProbe::normal(pipe),extent);
    const auto n=normal[at(12,3)];
    const float cosine=directionalFactor(blueLight(),{12,3},{n[0],n[1],n[2]});
    const auto directional=lightImage()[at(12,3)];
    near(directional[0],.25f,.001f,"normal mapping retains red shadow");
    near(directional[2],.25f+4*attenuation(blueLight(),{12,3})*cosine,.006f,"visible blue Lambert");
    check(directional[2]<target[2]-.1f,"measured directional difference");
    d.bloom=true; draw();
    check(maskImage()==red && s.passDraws[Extract]==1,"bloom preserves mask");
    check(read(PipelineProbe::bright(pipe),{65,49})[24*65+32][0]>0,"HDR emission enters bloom extraction");
    pic.occluders={}; draw(); const auto empty=lightImage();
    d.shadows=false; tools.previews=0; draw();
    check(!s.passDraws[ShadowBatch] && !s.shadowMask &&
          normal==read(PipelineProbe::normal(pipe),extent),"shadows off preserves NormalBuffer");
    const auto unmasked=lightImage();
    for (std::size_t i=0;i<empty.size();++i) for (int channel=0;channel<4;++channel)
        near(empty[i][channel],unmasked[i][channel],.008f,"unoccluded RGBA16F accumulation tolerance");
    d.shadows=true; pic.occluders=walls; d.view=View::Shadow;
    // Source in solid material, including a closed boundary, is completely blocked in range.
    for (float x:{0.f,-1.f}) {
        pic.lights.clear(); pic.lights.add(redLight(x)); draw();
        near(maskImage()[at(12,3)][0],0,0,"embedded source mask");
        near(lightImage()[at(-10,3)][0],.25f,.001f,"embedded source contributes zero direct light");
    }
}

std::array<GLint,6> blendState() {
    return {integer(GL_BLEND_EQUATION_RGB), integer(GL_BLEND_EQUATION_ALPHA),
            integer(GL_BLEND_SRC_RGB), integer(GL_BLEND_DST_RGB),
            integer(GL_BLEND_SRC_ALPHA), integer(GL_BLEND_DST_ALPHA)};
}

std::array<GLint,6> indexedBlendState(GLuint index) {
    std::array<GLint,6> result{};
    const GLenum names[]={GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA,GL_BLEND_SRC_RGB,GL_BLEND_DST_RGB,
                          GL_BLEND_SRC_ALPHA,GL_BLEND_DST_ALPHA};
    for (unsigned i=0;i<result.size();++i) glGetIntegeri_v(names[i],index,&result[i]);
    return result;
}
void resources(Window& window) {
    Display d; d.lighting=true; d.normalMapping=true; d.hud=false;
    Pipeline pipe; Picture p; p.lights.add(redLight(-20));
    p.occluders.addSegment(1,{0,-5},{0,10});
    Stats stats;
    pipe.resize(extent,true,true,false);
    check(pipe.shadowBytes()==0,"off starts without a shadow attachment");
    const auto scene=PipelineProbe::scene(pipe);
    const auto gen=pipe.generation();
    GLuint buffer=0;
    PipelineProbe::bufferAllocator(pipe,[&](std::size_t) {
        buffer=GLuint(integer(GL_ARRAY_BUFFER_BINDING));
        glBufferData(GL_ARRAY_BUFFER,-1,nullptr,GL_STREAM_DRAW);
    });
    rejects([&]{pipe.resize(extent,true,true,true);});
    check(buffer && !glIsBuffer(buffer) && pipe.generation()==gen && PipelineProbe::scene(pipe)==scene &&
          !pipe.shadowBytes(),"failed storage allocation retires temporary VBO and target");
    PipelineProbe::bufferAllocator(pipe,{});
    pipe.resize(extent,true,true,false); pipe.draw(p,d,stats);

    std::vector<GLuint> partial;
    auto failAllocation=[&](unsigned after) {
        partial.clear();
        PipelineProbe::checkpoint(pipe,[&,after](const auto& target) {
            partial.push_back(target.colorName());
            if (partial.size()==after) throw std::runtime_error("injected attachment failure");
        });
    };
    failAllocation(1); rejects([&]{pipe.resize(extent,true,true,true);});
    check(!glIsTexture(partial.at(0)) && !pipe.shadowBytes() && pipe.generation()==gen,"shadow enable rolls back");
    rejects([&]{pipe.draw(p,d,stats);}); // Failed resize disables draws until a matching retry.
    PipelineProbe::checkpoint(pipe,{});
    pipe.resize(extent,true,true,true); d.shadows=true;
    const auto mask=PipelineProbe::mask(pipe);
    glBindTexture(GL_TEXTURE_2D,mask); GLint format;
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);
    glBindTexture(GL_TEXTURE_2D,0);
    check(format==GL_RGBA8 && pipe.shadowBytes()==4ull*extent.width*extent.height,"linear RGBA8 mask accounting");
    failAllocation(1); rejects([&]{pipe.shadowPreview();});
    check(!glIsTexture(partial.at(0)) && !pipe.previewBytes(),"thumbnail allocation rolls back");
    PipelineProbe::checkpoint(pipe,{});
    const auto thumb=pipe.shadowPreview();
    pipe.previews(); pipe.lightPreview(); pipe.normalPreview();
    check(pipe.previewBytes()==7ull*extent.width*extent.height*4,"seven resident thumbnails");
    for (bool enable:{false,true,false,true}) {
        pipe.resize(extent,true,true,enable); d.shadows=enable; pipe.draw(p,d,stats);
        check(PipelineProbe::mask(pipe)==mask && pipe.generation()==gen && pipe.shadowBytes()>0,
              "same extent reuses and accounts for shadow attachment");
        if (!enable) rejects([&]{pipe.shadowPreview();});
    }

    glEnable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT,GL_MAX);
    glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_DST_ALPHA,GL_ZERO,GL_ONE);
    glColorMask(GL_FALSE,GL_TRUE,GL_FALSE,GL_TRUE);
    glEnable(GL_SCISSOR_TEST); glEnable(GL_DEPTH_TEST); glEnable(GL_FRAMEBUFFER_SRGB);
    GLuint callerBuffer; glGenBuffers(1,&callerBuffer); glBindBuffer(GL_ARRAY_BUFFER,callerBuffer);
    glViewport(3,7,17,19);
    const auto originalBlend=blendState();
    const bool indexed=integer(GL_MAJOR_VERSION)>=4 && integer(GL_MAX_DRAW_BUFFERS)>1;
    std::array<GLint,6> secondBlend{};
    if (indexed) {
        glBlendEquationSeparatei(1,GL_MIN,GL_FUNC_SUBTRACT);
        glBlendFuncSeparatei(1,GL_DST_COLOR,GL_ZERO,GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA);
        secondBlend=indexedBlendState(1);
    }
    auto restored=[&] {
        if (indexed) check(indexedBlendState(1)==secondBlend,"independent blend state of second draw buffer");
        GLboolean color[4]; glGetBooleanv(GL_COLOR_WRITEMASK,color);
        GLint vp[4]; glGetIntegerv(GL_VIEWPORT,vp);
        check(glIsEnabled(GL_BLEND) && blendState()==originalBlend && !color[0] && color[1] && !color[2] &&
              color[3] && vp[0]==3 && vp[1]==7 && vp[2]==17 && vp[3]==19 &&
              glIsEnabled(GL_SCISSOR_TEST) && glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_FRAMEBUFFER_SRGB) &&
              GLuint(integer(GL_ARRAY_BUFFER_BINDING))==callerBuffer,"restore blend, write mask, buffer and raster state");
    };
    DrawTools tools; tools.previews=127; tools.shadowSource=player; tools.epoch=player.epoch;
    pipe.draw(p,d,stats,tools); restored();
    tools.gui=[](Stats&){throw std::runtime_error("injected GUI submission failure");};
    rejects([&]{pipe.draw(p,d,stats,tools);}); restored();
    tools.gui={};
    failAllocation(12);
    rejects([&]{pipe.resize({153,111},true,true,true);}); restored();
    check(partial.size()==12 && std::none_of(partial.begin(),partial.end(),[](GLuint n){return glIsTexture(n);})
          && pipe.generation()==gen && glIsTexture(thumb.texture) && glIsTexture(mask),
          "replacement group fails atomically with valid previous borrows");
    rejects([&]{pipe.draw(p,d,stats,tools);}); rejects([&]{pipe.shadowPreview();});
    PipelineProbe::checkpoint(pipe,{});
    constexpr int edge=2340;
    static_assert(46ull*edge*edge+7*256*256*4+64*64*4<RenderTarget2D::budget &&
                  50ull*edge*edge>RenderTarget2D::budget);
    rejects([&]{pipe.resize({edge,edge},true,true,true);});
    check(!pipe.resize({0,0},true,true,true),"minimized shadow targets suspended");
    pipe.resize(extent,true,true,true); pipe.draw(p,d,stats,tools); restored();
    {
        Window other(20,20,"foreign shadow context");
        rejects([&]{pipe.resize(extent,true,true,true);}); rejects([&]{pipe.draw(p,d,stats,tools);});
        rejects([&]{pipe.shadowPreview();});
    }
    window.makeContextCurrent(); restored();
    for (Size next:{Size{257,193},Size{1,1}}) {
        pipe.resize(next,true,true,true); pipe.draw(p,d,stats,tools); restored();
        check(pipe.shadowPreview().generation==pipe.generation() && stats.shadowMask &&
              stats.shadowMask->generation==pipe.generation(),"new extent, current-frame preview");
        if (next.width==257) check(!glIsTexture(thumb.texture) && !glIsTexture(mask),"old names retired at resize");
    }
    glDeleteBuffers(1,&callerBuffer);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST); glDisable(GL_FRAMEBUFFER_SRGB); glDisable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD); glBlendFunc(GL_ONE,GL_ZERO); glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    detail::checkGL("shadow resource tests");
}

// Use the production fixed-tick update and renderer with a fixed camera. The third
// optional argument collects frames for animation outside the test working tree.
void motion(Window& window,const std::filesystem::path& stills,const std::filesystem::path& sequence) {
    const auto pixels=window.framebufferSize(), logical=window.size();
    Neon neon(71); Pipeline pipe;
    Display d; d.lighting=d.shadows=d.normalMapping=true; d.hud=false;
    d.radiusMultiplier=1.5f; d.ambient=.16f;
    Stats s; Picture pic; DrawTools tools;
    pipe.resize(pixels,true,true,true);
    for (unsigned tick=0;tick<=36;++tick) {
        if (tick) neon.update({-1,0,false});
        pic.prepare(neon,d,false,logical,pixels,s);
        pic.worldCamera.setPosition(-115,43);
        pic.worldCamera.setZoom(logical.width/235.f);
        if (!stills.empty() || tick==0 || tick==36) {
            pipe.draw(pic,d,s,tools);
            if (!stills.empty() && (tick==0 || tick==36))
                capture((stills/(tick?"player-after.png":"player-before.png")).string(),pixels);
            if (!sequence.empty()) capture((sequence/(std::to_string(100+tick)+".png")).string(),pixels);
        }
        if (tick==0 || tick==36) {
            const Vector2 sample{-120,49};
            const bool blocked=isOccluded(neon.player,sample,pic.occluders);
            check(blocked==(tick==0),"motion changes real Neon player visibility");
            const auto& lamp=*pic.lights.find({1,ObjectKind::Relay,2,1});
            check(!isOccluded(lamp.position,sample,pic.occluders) && attenuation(lamp,sample)>0,
                  "overlapping amber relay remains visible");
            std::cout<<"Neon seed=71 tick="<<tick<<" player=("<<neon.player.x<<','<<neon.player.y
                     <<") sample=(-120,49) player_visible="<<!blocked<<" amber_visible=1 fb="
                     <<pixels.width<<'x'<<pixels.height<<'\n';
        }
    }
}
}
int main(int argc,char** argv) {
    try {
        Window window(argc>1?800:180,argc>1?450:110,"M7.4b per-light occlusion");
        pixels(); resources(window);
        motion(window,argc>1?argv[1]:"",argc>2?argv[2]:"");
    } catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
