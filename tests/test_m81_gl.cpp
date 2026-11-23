#include "M5Test.h"
#include "FrameState.h"
#include "ParticleRenderer.h"
#include "Pipeline.h"
#include "ShaderAssets.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <type_traits>

using namespace lab;
using namespace tiny2d;

namespace lab {
struct PipelineProbe {
    static const auto& emission(const Pipeline& p) { return p.targets_->lighting->emission; }
    static const auto& scene(const Pipeline& p) { return p.targets_->scene; }
    static const auto& bright(const Pipeline& p) { return p.targets_->bright; }
    static const auto* renderer(const Pipeline& p) { return p.particleRenderer_.get(); }
    static GLuint vbo(const Pipeline& p) { return p.particleRenderer_->geometry_.instances.id(); }
    static void configure(Pipeline& p,const std::string& fragment,ParticleRenderer::Allocate allocate) {
        p.createParticleRenderer_=[fragment,allocate] {
            return std::unique_ptr<ParticleRenderer>(new ParticleRenderer(fragment,allocate));
        };
    }
    static void retry(Pipeline& p) { p.createParticleRenderer_={}; }
    static void submit(Pipeline& p,const ParticleRenderSnapshot& snap,const Camera2D& camera,Stats& s) {
        p.particleRenderer_->draw(snap,camera,s);
    }
};
}
namespace {
using Pixel=std::array<float,4>;
constexpr Size extent{129,97};
constexpr ParticleInstance source{0,0,18,0,1,.75f,.5f,4};
static_assert(ShadowInspect==15 && ParticleEmission==16);
static_assert(!std::is_copy_constructible_v<ParticleRenderer> && std::is_move_constructible_v<ParticleRenderer>);

GLint integer(GLenum name) { GLint n; glGetIntegerv(name,&n); return n; }

std::vector<Pixel> readImage(const RenderTarget2D* target,Size size) {
    const GLint previous=integer(GL_READ_FRAMEBUFFER_BINDING);
    GLuint fbo=0;
    if (target) {
        check(target->width()==size.width && target->height()==size.height,"fixture image extent");
        glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target->colorName(),0);
        check(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"fixture read FBO");
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    } else {
        glBindFramebuffer(GL_READ_FRAMEBUFFER,0); glReadBuffer(GL_BACK);
    }
    std::vector<Pixel> data(size.width*size.height);
    glReadPixels(0,0,size.width,size.height,GL_RGBA,GL_FLOAT,data.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER,previous);
    glDeleteFramebuffers(1,&fbo);
    detail::checkGL("particle fixture readback");
    return data;
}
void near(float a,float b,float tolerance,const char* label) {
    if (!(std::abs(a-b)<=tolerance)) {
        std::cerr<<label<<" observed="<<a<<" expected="<<b<<'\n';
        throw std::runtime_error(label);
    }
}
struct Fixture {
    Pipeline pipe;
    Picture pic;
    Display d;
    ParticleRenderSnapshot snap;
    DrawTools tools;
    Stats s;
    Fixture() {
        d.hud=false; d.lighting=true; d.bloom=false;
        pic.worldCamera.setViewportSize(extent.width,extent.height);
        tools.particles=&snap;
    }
    void draw(Size size=extent) {
        pipe.resize(size,d.lighting);
        pipe.draw(pic,d,s,tools);
        check(s.complete && s.draws==std::accumulate(s.passDraws.begin(),s.passDraws.end(),0u),"draws per pass");
    }
};

void pixels() {
    Fixture f;
    f.snap.instances={source};
    f.draw();
    const auto center=(extent.height/2)*extent.width+extent.width/2;
    auto emission=[&] { return readImage(&PipelineProbe::emission(f.pipe),extent); };
    const auto one=emission()[center];
    glBindTexture(GL_TEXTURE_2D,PipelineProbe::emission(f.pipe).colorName());
    GLint format=0; glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);
    glBindTexture(GL_TEXTURE_2D,0);
    check(format==GL_RGBA16F,"particle emission is linear RGBA16F");
    near(one[0],4,.01f,"red HDR output"); near(one[1],3,.01f,"green HDR output"); near(one[2],2,.01f,"blue HDR output");
    f.snap.instances.push_back(source); f.draw();
    const auto two=emission();
    for (unsigned i=0;i<3;++i) near(two[center][i],2*one[i],.01f,"overlapping particles add RGB");
    near(two[center][3],1,0,"emission alpha");
    const auto off=readImage(nullptr,extent);
    f.d.bloom=true; f.draw();
    const auto on=readImage(nullptr,extent);
    const auto& bright=PipelineProbe::bright(f.pipe);
    const auto highlights=readImage(&bright,{bright.width(),bright.height()});
    const auto high=highlights[bright.height()/2*bright.width()+bright.width()/2];
    check(high[0]>3 && high[1]>2,"bright extraction from fragment HDR");
    near(two[center+11][0],0,0,"sample outside the quad");
    check(on[center+11][0]>off[center+11][0]+.05f,"Bloom adds light outside particle footprint");
    check(emission()==two,"postprocessing preserves emission buffer");
    f.d.lighting=false; f.draw();
    const auto fallback=readImage(&PipelineProbe::scene(f.pipe),extent)[center];
    near(fallback[0],two[center][0]+.002f,.02f,"HDR scene fallback without lighting");

    // Distinct positions and colors detect non-advancing per-instance attributes.
    f.d.lighting=true; f.d.bloom=false;
    f.snap.instances={{-20,0,10,0,1,0,0,6},{20,0,10,0,0,1,0,5}};
    f.pic.worldCamera.setPosition(4,0);
    f.draw();
    const auto pair=emission();
    near(pair[center-24][0],6,.01f,"red particle follows camera");
    near(pair[center+16][1],5,.01f,"green particle has independent instance attributes");
    near(pair[center-24][1],0,0,"red contains no green");
    near(pair[center][0],0,0,"empty space between instances");
    f.snap.instances[0].ageFraction=1; f.draw();
    near(emission()[center-24][0],0,0,"lifetime fade reaches zero");

    RenderTarget2D alpha("fractional alpha",extent.width,extent.height);
    f.snap.instances={source,source};
    {
        FrameState state; alpha.bindDraw();
        constexpr float clear[]={.5f,.25f,0,.375f};
        glClearBufferfv(GL_COLOR,0,clear);
        PipelineProbe::submit(f.pipe,f.snap,f.pic.worldCamera,f.s);
        check(!glIsEnabled(GL_BLEND),"following pass starts with blending disabled");
    }
    const auto blended=readImage(&alpha,extent)[center-4];
    near(blended[0],8.5f,.01f,"fixed-function addition over colored destination");
    near(blended[3],.375f,0,"fractional destination alpha preserved with nonzero source coverage");
    std::cout<<"HDR one="<<one[0]<<','<<one[1]<<','<<one[2]<<" two="<<two[center][0]
             <<" alpha="<<blended[3]<<" bright="<<high[0]<<" bloom_off="<<off[center+11][0]
             <<" bloom_on="<<on[center+11][0]<<" lighting_off_hdr="<<fallback[0]<<'\n';
}

void populations() {
    Fixture f;
    BatchRenderer batch(2048,1); // The same capacity and texture budget as Lab's world batch.
    Texture2D white({1,1,{255,255,255,255}});
    GLuint query=0; glGenQueries(1,&query);
    for (unsigned count:{0u,1024u,4096u,8192u,0u}) {
        f.snap.instances.assign(count,source);
        const auto* previous=PipelineProbe::renderer(f.pipe);
        f.draw();
        check(f.s.activeInstances==count && f.s.instanceUploadBytes==count*sizeof(ParticleInstance),"exact instance payload");
        check(f.s.particleDrawCalls==unsigned(count>0) && f.s.passDraws[ParticleEmission]==f.s.particleDrawCalls,
              "one instanced draw for all nonempty workloads");
        if (!count) {
            check(previous==PipelineProbe::renderer(f.pipe) && f.s.instanceUploadUs==0 &&
                  !(f.s.mask&(1u<<ParticleEmission)),"zero skips allocation, upload, draw and pass");
        } else {
            check(!previous || PipelineProbe::renderer(f.pipe)==previous,"fixed storage reused across loads");
            FrameState state; PipelineProbe::emission(f.pipe).bindDraw();
            glBeginQuery(GL_PRIMITIVES_GENERATED,query);
            PipelineProbe::submit(f.pipe,f.snap,f.pic.worldCamera,f.s);
            glEndQuery(GL_PRIMITIVES_GENERATED);
            GLuint primitives=0; glGetQueryObjectuiv(query,GL_QUERY_RESULT,&primitives);
            check(primitives==2*count,"driver generated two triangles per instance");
        }
        batch.beginFrame(f.pic.worldCamera);
        for (unsigned i=0;i<count;++i) batch.draw(white,{0,0},{source.size,source.size});
        batch.endFrame();
        check(batch.stats().drawCalls==(count+2047)/2048,"measured ordinary batch draws");
        std::cout<<"particles="<<count<<" instance_draws="<<f.s.passDraws[ParticleEmission]
                 <<" instance_bytes="<<f.s.instanceUploadBytes<<" batch_draws="<<batch.stats().drawCalls
                 <<" batch_bytes="<<batch.stats().uploadedVertexBytes<<'\n';
    }
    glDeleteQueries(1,&query);
}

struct CallerState {
    ShaderProgram program{readShaderAsset("fullscreen.vert"),readShaderAsset("display.frag")};
    GLuint vao{},buffer{},drawFbo{},readFbo{};
    std::array<GLuint,3> textures{},samplers{};
    CallerState() {
        glGenVertexArrays(1,&vao); glGenBuffers(1,&buffer);
        glGenFramebuffers(1,&drawFbo); glGenFramebuffers(1,&readFbo);
        glGenTextures(3,textures.data()); glGenSamplers(3,samplers.data());
        glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,buffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,drawFbo); glBindFramebuffer(GL_READ_FRAMEBUFFER,readFbo);
        for (unsigned i=0;i<3;++i) {
            glActiveTexture(GL_TEXTURE0+i); glBindTexture(GL_TEXTURE_2D,textures[i]); glBindSampler(i,samplers[i]);
        }
        glActiveTexture(GL_TEXTURE2);
        glEnable(GL_BLEND); glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT,GL_MAX);
        glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_DST_ALPHA,GL_ZERO);
        glViewport(3,5,19,23); program.bind();
    }
    void verify() const {
        GLint viewport[4]; glGetIntegerv(GL_VIEWPORT,viewport);
        check(viewport[0]==3 && viewport[1]==5 && viewport[2]==19 && viewport[3]==23,"caller viewport");
        check(GLuint(integer(GL_DRAW_FRAMEBUFFER_BINDING))==drawFbo && GLuint(integer(GL_READ_FRAMEBUFFER_BINDING))==readFbo,
              "both framebuffer bindings");
        check(GLuint(integer(GL_VERTEX_ARRAY_BINDING))==vao && GLuint(integer(GL_ARRAY_BUFFER_BINDING))==buffer &&
              GLuint(integer(GL_CURRENT_PROGRAM))==program.nativeHandle(),"caller VAO/VBO/program");
        check(glIsEnabled(GL_BLEND) && integer(GL_BLEND_EQUATION_RGB)==GL_FUNC_REVERSE_SUBTRACT &&
              integer(GL_BLEND_EQUATION_ALPHA)==GL_MAX && integer(GL_BLEND_SRC_RGB)==GL_SRC_ALPHA &&
              integer(GL_BLEND_DST_RGB)==GL_ONE_MINUS_SRC_ALPHA && integer(GL_BLEND_SRC_ALPHA)==GL_DST_ALPHA &&
              integer(GL_BLEND_DST_ALPHA)==GL_ZERO,"caller blend enable, equations and four factors");
        check(integer(GL_ACTIVE_TEXTURE)==GL_TEXTURE2,"caller active texture");
        for (unsigned i=0;i<3;++i) {
            glActiveTexture(GL_TEXTURE0+i);
            check(GLuint(integer(GL_TEXTURE_BINDING_2D))==textures[i] &&
                  GLuint(integer(GL_SAMPLER_BINDING))==samplers[i],"caller textures and samplers");
        }
        detail::checkGL("external GL state fixture");
    }
    ~CallerState() {
        glBindFramebuffer(GL_FRAMEBUFFER,0); glDeleteFramebuffers(1,&drawFbo); glDeleteFramebuffers(1,&readFbo);
        glDeleteTextures(3,textures.data()); glDeleteSamplers(3,samplers.data());
        glDeleteBuffers(1,&buffer); glDeleteVertexArrays(1,&vao);
        glDisable(GL_BLEND); glBlendEquation(GL_FUNC_ADD); glBlendFunc(GL_ONE,GL_ZERO);
        glActiveTexture(GL_TEXTURE0);
    }
};

void resources(Window& window) {
    Fixture f;
    CallerState caller;
    f.snap.instances={source};
    const auto fragment=readShaderAsset("particle.frag");
    for (bool badShader:{false,true}) {
        GLuint vao=0,quad=0,instances=0;
        PipelineProbe::configure(f.pipe,badShader ? "bad GLSL" : fragment,[&](std::size_t bytes) {
            vao=GLuint(integer(GL_VERTEX_ARRAY_BINDING));
            instances=GLuint(integer(GL_ARRAY_BUFFER_BINDING));
            GLint name; glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&name); quad=GLuint(name);
            glBufferData(GL_ARRAY_BUFFER,badShader ? GLsizeiptr(bytes) : -1,nullptr,GL_STREAM_DRAW);
        });
        std::string diagnostic;
        try { f.draw(); } catch (const std::exception& e) { diagnostic=e.what(); }
        check(diagnostic.find(badShader ? "particle.frag: compilation failed" : "particle VBO storage")!=std::string::npos,
              "reported actual injected driver failure");
        check(vao && quad && instances && !glIsVertexArray(vao) && !glIsBuffer(quad) && !glIsBuffer(instances) &&
              !PipelineProbe::renderer(f.pipe) && !f.s.complete && !f.s.particleDrawCalls,
              "failed creation retires partial shared quad, instance VBO and VAO");
        caller.verify();
    }
    PipelineProbe::retry(f.pipe); f.draw(); caller.verify();
    const auto vbo=PipelineProbe::vbo(f.pipe);
    for (Size size:{Size{1,1},Size{131,77},extent}) {
        f.draw(size); caller.verify();
        check(PipelineProbe::vbo(f.pipe)==vbo && f.s.particleDrawCalls==1,"resize reuses particle assets");
    }
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    check(integer(GL_ARRAY_BUFFER_BINDING)==GLint(vbo),"fixture bound instance VBO");
    GLint bytes=0; glGetBufferParameteriv(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&bytes);
    check(std::size_t(bytes)==ParticleSimulation::maxCapacity*sizeof(ParticleInstance),"reserved instance storage");
    glBindBuffer(GL_ARRAY_BUFFER,caller.buffer);

    f.snap.instances[0].x=std::numeric_limits<float>::quiet_NaN();
    rejects([&]{f.draw();});
    check(f.s.instanceUploadBytes==0 && !f.s.complete,"NaN rejected before uploading instance memory");
    caller.verify();
    f.snap.instances.assign(ParticleSimulation::maxCapacity+1,source); rejects([&]{f.draw();});
    check(f.s.instanceUploadBytes==0 && !f.s.particleDrawCalls,"reject oversized snapshot before upload");
    f.snap.instances={source};
    {
        Window foreign(24,24,"M8.1 foreign context");
        rejects([&]{f.pipe.draw(f.pic,f.d,f.s,f.tools);});
        rejects([&]{PipelineProbe::submit(f.pipe,f.snap,f.pic.worldCamera,f.s);});
    }
    window.makeContextCurrent(); caller.verify();
    f.draw(); caller.verify();
}

void replayPixels() {
    Fixture f;
    f.pic.worldCamera.setViewportSize(180,140);
    auto render=[&](unsigned ticks) {
        ParticleSimulation sim(71,1024);
        for (unsigned i=0;i<ticks;++i) {
            ParticleTickInput input;
            if (i==10) input.burst=64;
            sim.step(input);
        }
        sim.snapshot(f.snap);
        f.draw();
        return readImage(nullptr,extent);
    };
    const auto a=render(75);
    check(a==render(75),"same seed and deterministic tick inputs reproduce the back buffer");
    check(a!=render(90),"finite tick advancement changes particle rendering");
}
}
int main() {
    try {
        Window window(160,120,"M8.1 HDR instanced particles");
        pixels(); populations(); resources(window); replayPixels();
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
