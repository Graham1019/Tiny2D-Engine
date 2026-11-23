#include "M5Test.h"
#include "Capture.h"
#include "OpenGL.h"
#include "ShaderAssets.h"
#include "ShaderGallery.h"
#include "ShaderPanel.h"
#include "imgui_impl_glfw.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <future>
#include <unistd.h>

namespace lab {
struct PipelineProbe {
    static const auto& scene(const Pipeline& p) { return p.targets_->scene; }
};
}

namespace {
using namespace lab;
using namespace tiny2d;
namespace fs = std::filesystem;
using Pixel = std::array<float,4>;
using Image = std::vector<Pixel>;
constexpr Size extent{129,97};
static_assert(ShadowInspect==15 && ParticleEmission==16 && ShaderGalleryPass==17);

GLint binding(GLenum name) {
    GLint value=0;
    glGetIntegerv(name,&value);
    return value;
}

Image readHDR(const Pipeline& pipe) {
    const auto& scene=PipelineProbe::scene(pipe);
    const auto old=binding(GL_TEXTURE_BINDING_2D);
    glBindTexture(GL_TEXTURE_2D,scene.colorName());
    GLint format=0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);
    Image pixels(std::size_t(scene.width())*scene.height());
    glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,pixels.data());
    glBindTexture(GL_TEXTURE_2D,old);
    detail::checkGL("gallery HDR readback");
    check(format==GL_RGBA16F,"gallery scene format");
    return pixels;
}

Image readBack(Size size) {
    check(binding(GL_READ_FRAMEBUFFER_BINDING)==0,"readback requires default framebuffer");
    glReadBuffer(GL_BACK);
    Image pixels(std::size_t(size.width)*size.height);
    glReadPixels(0,0,size.width,size.height,GL_RGBA,GL_FLOAT,pixels.data());
    detail::checkGL("gallery display readback");
    return pixels;
}

// Fraction of pixels whose RGB changes by more than tolerance on any channel.
double changed(const Image& a,const Image& b,float tolerance) {
    check(a.size()==b.size() && !a.empty(),"comparable images");
    std::size_t count=0;
    for (std::size_t i=0;i<a.size();++i)
        for (unsigned c=0;c<3;++c)
            if (std::abs(a[i][c]-b[i][c])>tolerance) { ++count; break; }
    return double(count)/a.size();
}

struct Frame {
    Pipeline pipe;
    Display display;
    DrawTools draw;
    Picture picture;
    Stats stats;
    Frame() { display.hud=false; display.bloom=false; }
    void render(const ShaderReloadManager& shader,std::uint64_t tick,Size size=extent) {
        check(pipe.resize(size),"expected drawable extent");
        draw.gallery=&shader;
        draw.tick=tick;
        pipe.draw(picture,display,stats,draw);
        check(stats.complete && stats.passDraws[ShaderGalleryPass]==1 && stats.passDraws[Output]==1 &&
              !(stats.mask & ((1u<<World)|(1u<<ParticleEmission))),"independent gallery pass");
    }
};

float encoded(float hdr) {
    const float mapped=hdr/(1+hdr);
    return mapped<=.0031308f ? 12.92f*mapped : 1.055f*std::pow(mapped,1.f/2.4f)-.055f;
}

void exhibits() {
    ShaderGallery gallery;
    Frame frame;
    std::vector<Image> exhibited;
    for (const auto effect:galleryEffects) {
        auto& shader=gallery.shader(effect);
        frame.display.bloom=false;
        frame.render(shader,90);
        auto hdr=readHDR(frame.pipe);
        auto off=readBack(extent);
        float peak=0,displayError=0;
        for (std::size_t i=0;i<hdr.size();++i) {
            check(hdr[i][3]==1.f,"opaque procedural color");
            for (unsigned c=0;c<3;++c) {
                check(std::isfinite(hdr[i][c]) && hdr[i][c]>=0,"finite nonnegative linear RGB");
                peak=std::max(peak,hdr[i][c]);
                displayError=std::max(displayError,std::abs(off[i][c]-encoded(hdr[i][c])));
            }
        }
        check(peak>1 && displayError<.012f,"HDR highlights and common Reinhard/sRGB display");
        frame.display.bloom=true;
        frame.render(shader,90);
        auto on=readBack(extent);
        const auto bloomChange=changed(off,on,.01f);
        check(bloomChange>.005 && readHDR(frame.pipe)==hdr,"Bloom modifies final pixels while retaining the HDR scene");
        for (auto pass:{Extract,Horizontal,Vertical})
            check(frame.stats.passDraws[pass]==1,"gallery reuses each production Bloom pass");
        frame.render(shader,150);
        const auto tickChange=changed(hdr,readHDR(frame.pipe),.02f);
        check(tickChange>.015,"one simulated second changes the procedural image");
        frame.render(shader,90);
        check(readHDR(frame.pipe)==hdr && readBack(extent)==on,"identical tick reproduces both buffers");
        for (const auto& previous:exhibited)
            check(changed(previous,hdr,.1f)>.2,"different gallery exhibits must differ across the image");
        exhibited.push_back(std::move(hdr));
        std::cout<<"effect="<<galleryName(effect)<<" tick=90 hdr_peak="<<peak
                 <<" max_display_error="<<displayError<<" bloom_changed_fraction="<<bloomChange
                 <<" tick_changed_fraction="<<tickChange<<'\n';
    }
}

struct Scratch {
    fs::path dir,vertex,fragment;
    Scratch() {
        auto pattern=(fs::temp_directory_path()/"m82-gallery-XXXXXX").string();
        if (!mkdtemp(pattern.data())) throw std::runtime_error("cannot create shader fixture directory");
        dir=pattern;
        vertex=dir/"screen.vert";
        fragment=dir/"aurora.frag";
        fs::copy_file(assetPath("shaders/gallery/screen.vert"),vertex);
        fs::copy_file(assetPath("shaders/gallery/aurora.frag"),fragment);
    }
    ~Scratch() { std::error_code e; fs::remove_all(dir,e); }
};

void write(const fs::path& path,const std::string& text) {
    std::ofstream out(path,std::ios::binary);
    out.exceptions(std::ios::failbit|std::ios::badbit);
    out<<text;
    out.close();
}

void rejected(ShaderReloadManager& shader,std::string_view reason) {
    const auto old=shader.program().nativeHandle();
    const auto version=shader.status().version;
    const auto caller=binding(GL_CURRENT_PROGRAM);
    check(!shader.reload(),"bad candidate unexpectedly installed");
    const auto& status=shader.status();
    if (status.error.find(reason)==std::string::npos)
        throw std::runtime_error("expected diagnostic containing '"+std::string(reason)+"', received '"+status.error+"'");
    check(!status.success && status.version==version && old==shader.program().nativeHandle() &&
          glIsProgram(old) && binding(GL_CURRENT_PROGRAM)==caller,"rollback preserves version, resource and caller binding");
    detail::checkGL("after candidate rollback");
}

constexpr char invalid[]="#version 330 core\nvoid main() { broken GLSL; }\n";

void lifecycle(Window& owner) {
    Scratch assets;
    Frame frame;
    const auto aurora=readShaderAsset("gallery/aurora.frag");
    const auto contours=readShaderAsset("gallery/contours.frag");
    const auto vertex=readShaderAsset("gallery/screen.vert");
    GLuint retiredOnExit=0;
    {
        ShaderReloadManager shader(assets.vertex,assets.fragment);
        check(shader.status().version==1 && shader.status().success &&
              shader.vertexPath()==assets.vertex && shader.fragmentPath()==assets.fragment,"initial gallery version");
        ShaderProgram external(readShaderAsset("fullscreen.vert"),readShaderAsset("display.frag"));
        external.bind();
        frame.render(shader,90);
        const auto original=readHDR(frame.pipe);
        write(assets.fragment,invalid);
        rejected(shader,"compilation failed");
        check(shader.status().error.find(assets.fragment.string())!=std::string::npos,"full failed-stage path");
        frame.render(shader,90);
        check(readHDR(frame.pipe)==original,"compile failure preserves exact HDR pixels");

        // Both stages compile individually; the same active uniform has incompatible types at link.
        write(assets.vertex,"#version 330 core\nuniform vec2 uTime;\nvoid main(){gl_Position=vec4(uTime,0,1);}");
        const std::string prefix="#version 330 core\nlayout(location=0) out vec4 fragColor;\n";
        write(assets.fragment,prefix+"uniform float uTime; uniform vec2 uResolution;"
              "void main(){fragColor=vec4(uResolution,uTime,1);}");
        rejected(shader,"link failed");
        write(assets.vertex,vertex);
        write(assets.fragment,prefix+"uniform float uTime; uniform vec2 uResolution;"
              "void main(){fragColor=vec4(uResolution,0,1);}"); // uTime optimized out.
        rejected(shader,"uTime");
        write(assets.fragment,prefix+"uniform vec2 uTime; uniform vec2 uResolution;"
              "void main(){fragColor=vec4(uTime,uResolution);}");
        rejected(shader,"uTime");
        write(assets.fragment,prefix+"uniform float uTime[2]; uniform vec2 uResolution;"
              "void main(){fragColor=vec4(uResolution,uTime[0],uTime[1]);}");
        rejected(shader,"uTime");
        write(assets.fragment,prefix+"uniform float uTime; uniform vec2 uResolution; uniform sampler2D tex;"
              "void main(){fragColor=texture(tex,gl_FragCoord.xy/uResolution)+vec4(uTime);}");
        rejected(shader,"tex");
        write(assets.fragment,"#version 330 core\nlayout(location=1) out vec4 fragColor;"
              "uniform float uTime; uniform vec2 uResolution;"
              "void main(){fragColor=vec4(uResolution,uTime,1);}");
        rejected(shader,"fragColor");
        write(assets.fragment,aurora);
        write(assets.vertex,"#version 330 core\nin vec2 vertexPosition;"
              "void main(){gl_Position=vec4(vertexPosition,0,1);}");
        rejected(shader,"vertexPosition");
        write(assets.vertex,vertex);
        fs::remove(assets.fragment);
        rejected(shader,assets.fragment.string());
        frame.render(shader,90);
        check(readHDR(frame.pipe)==original,"link, interface and asset failures preserve exact HDR pixels");

        const auto lastError=shader.status().error;
        {
            struct RestoreContext {
                Window& window;
                ~RestoreContext() { window.makeContextCurrent(); }
            } restore{owner};
            Window other(32,32,"Foreign gallery context");
            rejects([&]{shader.reload();});
            Frame foreignFrame;
            const Size small{31,23};
            foreignFrame.pipe.resize(small);
            ShaderProgram caller(readShaderAsset("fullscreen.vert"),readShaderAsset("display.frag"));
            detail::GLObject vao(detail::ObjectKind::VertexArray), fbo(detail::ObjectKind::Framebuffer);
            caller.bind();
            glBindVertexArray(vao.id());
            glBindFramebuffer(GL_FRAMEBUFFER,fbo.id());
            glEnable(GL_SCISSOR_TEST);
            glViewport(1,3,7,9);
            foreignFrame.draw.gallery=&shader;
            rejects([&]{foreignFrame.pipe.draw(foreignFrame.picture,foreignFrame.display,
                                               foreignFrame.stats,foreignFrame.draw);});
            std::array<GLint,4> viewport{};
            glGetIntegerv(GL_VIEWPORT,viewport.data());
            check(!foreignFrame.stats.complete && !foreignFrame.stats.passDraws[ShaderGalleryPass] &&
                  GLuint(binding(GL_CURRENT_PROGRAM))==caller.nativeHandle() && glIsEnabled(GL_SCISSOR_TEST) &&
                  GLuint(binding(GL_VERTEX_ARRAY_BINDING))==vao.id() &&
                  GLuint(binding(GL_DRAW_FRAMEBUFFER_BINDING))==fbo.id() &&
                  GLuint(binding(GL_READ_FRAMEBUFFER_BINDING))==fbo.id() &&
                  viewport==std::array<GLint,4>{1,3,7,9},"foreign shader borrow restores caller state on unwind");
            detail::checkGL("foreign shader draw unwind");
        }
        check(std::async(std::launch::async,[&] {
            try { shader.reload(); } catch (const std::logic_error&) { return true; }
            return false;
        }).get(),"worker reload rejected");
        check(shader.status().version==1 && shader.status().error==lastError &&
              GLuint(binding(GL_CURRENT_PROGRAM))==external.nativeHandle(),"wrong owner leaves status/binding unchanged");

        for (unsigned cycle=0;cycle<6;++cycle) {
            if (cycle%2==0) external.bind(); else shader.program().bind();
            auto old=shader.program().nativeHandle();
            write(assets.fragment,invalid);
            rejected(shader,"compilation failed");
            write(assets.fragment,cycle%2==0 ? contours : aurora);
            check(shader.reload() && shader.status().success && shader.status().error.empty() &&
                  shader.status().version==cycle+2,"successful reload advances version exactly once");
            auto current=shader.program().nativeHandle();
            check(current!=old && glIsProgram(current) && !glIsProgram(old),"successful candidate replaces and retires old GL name");
            check(GLuint(binding(GL_CURRENT_PROGRAM))==(cycle%2==0 ? external.nativeHandle() : current),
                  "external binding retained or bound exhibit migrated to its replacement");
            frame.render(shader,90);
            const auto image=readHDR(frame.pipe);
            if (cycle%2==0) check(changed(image,original,.1f)>.2,"new shader changes rendered pixels");
            else check(image==original,"restored source reproduces exact pixels");
        }
        retiredOnExit=shader.program().nativeHandle();
        shader.program().bind();
    }
    check(!glIsProgram(retiredOnExit) && binding(GL_CURRENT_PROGRAM)==0,"owning context retires final bound program");
    std::cout<<"rollback=compile,link,inactive,type,array,sampler,attribute,output_location,file"
             <<" wrong_context=blocked foreign_draw_state=restored worker=blocked"
             <<" alternating_commits=6 exact_HDR_restored=yes bound_program_retired=yes\n";
}

void resizeAndState() {
    ShaderGallery gallery;
    auto& shader=gallery.shader(GalleryEffect::Orbitals);
    Frame frame;
    ShaderProgram external(readShaderAsset("fullscreen.vert"),readShaderAsset("display.frag"));
    external.bind();
    detail::GLObject vao(detail::ObjectKind::VertexArray), buffer(detail::ObjectKind::Buffer),
                     readFbo(detail::ObjectKind::Framebuffer), drawFbo(detail::ObjectKind::Framebuffer),
                     texture(detail::ObjectKind::Texture);
    glBindVertexArray(vao.id());
    glBindBuffer(GL_ARRAY_BUFFER,buffer.id());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,drawFbo.id());
    glBindFramebuffer(GL_READ_FRAMEBUFFER,readFbo.id());
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D,texture.id());
    glEnable(GL_BLEND);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0,0,0,0);
    glViewport(4,7,13,17);
    const auto program=shader.program().nativeHandle();
    for (Size size:{extent,Size{1,1},Size{77,121},extent}) {
        frame.render(shader,120,size);
        check(binding(GL_CURRENT_PROGRAM)==GLint(external.nativeHandle()) &&
              binding(GL_VERTEX_ARRAY_BINDING)==GLint(vao.id()) &&
              binding(GL_ARRAY_BUFFER_BINDING)==GLint(buffer.id()) &&
              binding(GL_READ_FRAMEBUFFER_BINDING)==GLint(readFbo.id()) &&
              binding(GL_DRAW_FRAMEBUFFER_BINDING)==GLint(drawFbo.id()) &&
              binding(GL_ACTIVE_TEXTURE)==GL_TEXTURE2 &&
              binding(GL_TEXTURE_BINDING_2D)==GLint(texture.id()) &&
              glIsEnabled(GL_BLEND) && glIsEnabled(GL_SCISSOR_TEST),"gallery restores surrounding GL state");
        std::array<GLint,4> viewport{};
        glGetIntegerv(GL_VIEWPORT,viewport.data());
        check(viewport==std::array<GLint,4>{4,7,13,17},"restore caller viewport");
        check(shader.program().nativeHandle()==program && shader.status().version==1,"resize retains program");
        GLfloat resolution[2]{},time=0;
        glGetUniformfv(program,glGetUniformLocation(program,"uResolution"),resolution);
        glGetUniformfv(program,glGetUniformLocation(program,"uTime"),&time);
        check(resolution[0]==size.width && resolution[1]==size.height && time==2.f,
              "driver sees framebuffer extent and tick/60 seconds");
        check(!frame.pipe.resize({0,size.height}),"suspend at zero extent");
        rejects([&]{frame.pipe.draw(frame.picture,frame.display,frame.stats,frame.draw);});
    }
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND); glActiveTexture(GL_TEXTURE0);
    detail::checkGL("gallery resize and state");
}

void galleryInput(Window& window,bool finite) {
    Inspector gui(window,finite);
    ToolState tools{false,false,false};
    Display display;
    Controls controls;
    auto poll=[&](bool gallery) {
        auto routed=gui.buildPanel(tools,true,[](Commands&){},gallery);
        const auto action=controls.poll(routed.keys,true,0,display,routed.captureKeyboard,routed.commands,routed.freshFocus);
        gui.discard();
        return action.pressed[ReloadShader];
    };
    auto key=[&](int event){ImGui_ImplGlfw_KeyCallback(window.nativeHandle(),GLFW_KEY_F5,0,event,0);};
    poll(true); poll(true);
    key(GLFW_PRESS);
    check(poll(true)==!finite && !poll(true),"one F5 edge when interactive, none when finite");
    key(GLFW_RELEASE); poll(true);
    key(GLFW_PRESS); check(!poll(false),"legacy scenes ignore F5");
    key(GLFW_RELEASE); poll(false);
}

void recoveryScreens(Window& window,const fs::path& output) {
    fs::create_directories(output);
    Scratch files;
    ShaderReloadManager shader(files.vertex,files.fragment);
    Inspector gui(window,false);
    ToolState tools{true,false,false};
    GalleryEffect effect=GalleryEffect::Aurora;
    Frame frame;
    frame.display.bloom=true;
    const auto pixels=window.framebufferSize();
    FrameInfo info; info.tick=90; info.logical=window.size(); info.pixels=pixels; info.display=frame.display;
    auto screenshot=[&](const char* name) {
        ++info.id;
        gui.buildPanel(tools,true,[&](Commands& cmd) {
            shaderPanel(tools,cmd,effect,shader,info.tick,true,frame.display,false);
        },true);
        gui.finish(tools,frame.pipe,info,frame.picture);
        frame.draw.gui=[&](Stats& stats){gui.submit(stats);};
        frame.render(shader,info.tick,pixels);
        capture((output/name).string(),pixels);
    };
    screenshot("reload-before.png");
    const auto baseline=readHDR(frame.pipe);
    write(files.fragment,invalid);
    rejected(shader,"compilation failed");
    screenshot("reload-failed.png");
    check(readHDR(frame.pipe)==baseline,"Inspector compile-error capture retains the same HDR pixels");
    write(files.fragment,readShaderAsset("gallery/contours.frag"));
    check(shader.reload(),"edited exhibit recovers");
    screenshot("reload-recovered.png");
    check(changed(readHDR(frame.pipe),baseline,.1f)>.2,"recovery evidence captured changed shader content");
}
}

int main(int argc,char** argv) {
    try {
        check(argc<=2,"expected at most an evidence output directory");
        Window window(880,540,"M8.2 Shader Gallery");
        window.pollEvents();
        std::cout<<"GL="<<glGetString(GL_VERSION)<<" renderer="<<glGetString(GL_RENDERER)<<'\n';
        exhibits(); lifecycle(window); resizeAndState();
        galleryInput(window,false); galleryInput(window,true);
        if (argc==2) recoveryScreens(window,fs::absolute(argv[1]));
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
