#include "Pipeline.h"
#include "OpenGL.h"
#include "tiny2d/Input.h"
#include <png.h>
#include <algorithm>
#include <cmath>
#include <iostream>


namespace lab {
namespace {
Keys readKeys(const tiny2d::Input& input) {
    constexpr std::array<int,KeyCount> codes{{GLFW_KEY_A,GLFW_KEY_D,GLFW_KEY_W,GLFW_KEY_S,
        GLFW_KEY_SPACE,GLFW_KEY_B,GLFW_KEY_1,GLFW_KEY_2,GLFW_KEY_3,GLFW_KEY_4,
        GLFW_KEY_LEFT_BRACKET,GLFW_KEY_RIGHT_BRACKET,GLFW_KEY_P,GLFW_KEY_PERIOD,GLFW_KEY_R,
        GLFW_KEY_F1,GLFW_KEY_ESCAPE}};
    Keys result;
    for (unsigned i=0;i<codes.size();++i) result[i]=input.isKeyPressed(codes[i]);
    return result;
}
// CLI-only capture of the final back buffer, outside sampling, before swap.
// The application owns this context's default read binding and pixel-pack state.
void capture(const std::string& path,tiny2d::Size size) {
    std::vector<png_byte> pixels(std::size_t(size.width)*size.height*4);
    glReadBuffer(GL_BACK);
    glReadPixels(0,0,size.width,size.height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    tiny2d::detail::checkGL("Lab capture");
    png_image image{};
    image.version=PNG_IMAGE_VERSION;
    image.width=size.width; image.height=size.height; image.format=PNG_FORMAT_RGBA;
    if (!png_image_write_to_file(&image,path.c_str(),0,pixels.data(),-size.width*4,nullptr))
        throw std::runtime_error("PNG capture "+path+": "+image.message);
}
struct Sample { double update,prepare,frame; Stats stats; };
void report(const std::vector<Sample>& samples) {
    if (samples.empty()) return;
    auto metric=[&](const char* name,auto value) {
        std::vector<double> sorted;
        for (const auto& sample:samples) sorted.push_back(value(sample));
        std::sort(sorted.begin(),sorted.end());
        auto percentile=[&](double p){ return sorted[std::size_t(std::ceil(p*sorted.size()))-1]; };
        std::cout<<"metric="<<name<<" p50="<<percentile(.50)<<" p95="<<percentile(.95)
                 <<" p99="<<percentile(.99)<<'\n';
    };
    std::cout<<"warmup=120 samples="<<samples.size()<<" clock=steady units=us\n";
    metric("fixed_update",[](const Sample& s){return s.update;});
    metric("prepare",[](const Sample& s){return s.prepare;});
    constexpr const char* passes[]={"scene","bright","horizontal","vertical","output","hud"};
    for (unsigned i=0;i<PassCount;++i)
        metric(passes[i],[&](const Sample& s){return s.stats.cpuUs[i];});
    metric("frame",[](const Sample& s){return s.frame;});
    metric("draw_calls",[](const Sample& s){return s.stats.draws;});
    metric("sprites",[](const Sample& s){return s.stats.sprites;});
    metric("uploaded_bytes",[](const Sample& s){return s.stats.bytes;});
}
void run(Options o) {
    Neon neon(o.seed);
    Controls controls;
    Picture picture;
    Stats last, attempt;
    std::uint64_t frames=0, storage=0;
    tiny2d::Size logical{}, pixels{};
    auto summary=[&](const char* status) {
        std::cout<<"status="<<status<<" frames="<<frames<<" ticks="<<neon.tick<<" seed="<<o.seed
                 <<" window="<<logical.width<<'x'<<logical.height<<" framebuffer="<<pixels.width<<'x'<<pixels.height
                 <<" view="<<name(o.display.view)<<" bloom="<<(o.display.bloom?"on":"off")
                 <<" strength="<<o.display.strength/10.f<<" last_passes="<<last.passes
                 <<" fullscreen="<<last.fullscreen<<" draws="<<last.draws<<" sprites="<<last.sprites
                 <<" upload_bytes="<<last.bytes<<" rgba16f_attachment_bytes="<<storage
                 <<" attempt_passes="<<attempt.passes<<" attempt_draws="<<attempt.draws
                 <<" attempt_complete="<<attempt.complete<<'\n';
    };
    try {
        tiny2d::Window window(o.width,o.height,"Tiny2D Lab / Neon");
        tiny2d::Input input(window);
        if (o.frames) glfwSwapInterval(0);
        GLint profile;
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK,&profile);
        std::cout<<"profile="<<((profile & GL_CONTEXT_CORE_PROFILE_BIT)?"core":"other")<<" opengl="<<glGetString(GL_VERSION)<<" renderer="<<glGetString(GL_RENDERER)<<'\n';
        Pipeline renderer;
        std::vector<Sample> samples;
        samples.reserve(1200);
        auto previous=Clock::now();
        Clock::duration waited{};
        std::optional<Clock::time_point> unavailableSince;
        std::string resizeError;
        while (!o.frames || frames<*o.frames) {
            window.pollEvents();
            if (window.shouldClose()) {
                if (o.frames) throw std::runtime_error("window closed before requested frames");
                break;
            }
            const auto start=Clock::now();
            double dt=std::chrono::duration<double>(start-previous).count();
            previous=start;
            logical=window.size(); pixels=window.framebufferSize();
            attempt={};
            bool drawable=false;
            if (logical.width>0 && logical.height>0 && !window.iconified()) {
                try {
                    drawable=renderer.resize(pixels);
                    resizeError.clear();
                } catch (const std::exception& e) {
                    if (o.frames) throw;
                    if (resizeError!=e.what()) std::cerr<<"resize: "<<e.what()<<'\n';
                    resizeError=e.what();
                }
            }
            if (unavailableSince && o.frames && waited+(start-*unavailableSince) >= std::chrono::seconds(5))
                throw std::runtime_error("drawable wait exceeded 5 cumulative seconds");
            if (!drawable) {
                if (!unavailableSince) unavailableSince=start;
                controls.poll({},false,0,o.display);
                glfwWaitEventsTimeout(.05);
                continue;
            }
            if (unavailableSince) {
                waited+=start-*unavailableSince;
                unavailableSince.reset();
            }
            Action action;
            if (o.frames) action.steps=1;
            else action=controls.poll(readKeys(input),input.focused(),dt,o.display);
            if (action.exit) break;
            if (action.reset) neon=Neon(o.seed);
            const auto updateStart=Clock::now();
            for (unsigned i=0;i<action.steps;++i) neon.update(action.motion);
            const double update=micros(updateStart);
            const auto prepareStart=Clock::now();
            picture.prepare(neon,o.display,controls.paused,logical,pixels,last);
            const double prepare=micros(prepareStart);
            storage=renderer.storage();
            renderer.draw(picture,o.display,attempt);
            const double frame=micros(start);
            if (o.frames && frames+1==*o.frames && !o.capture.empty()) capture(o.capture,pixels);
            window.swapBuffers();
            const char* error=nullptr;
            if (glfwGetError(&error)!=GLFW_NO_ERROR)
                throw std::runtime_error(std::string("window submission: ")+(error?error:"unknown error"));
            ++frames;
            last=attempt;
            if (o.frames && frames>120 && samples.size()<1200) samples.push_back({update,prepare,frame,last});
        }
        report(samples);
    } catch (...) {
        summary("error");
        throw;
    }
    summary("ok");
}
}
}
int main(int argc,char** argv) {
    try { lab::run(lab::parse(argc,argv)); }
    catch (const std::exception& e) { std::cerr<<"Tiny2D Lab: "<<e.what()<<'\n'; return 1; }
}
