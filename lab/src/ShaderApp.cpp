#include "ShaderApp.h"
#include "ShaderGallery.h"
#include "ShaderWorkspace.h"
#include "tiny2d/OutputFiles.h"
#include "ShaderAssets.h"
#include "ShaderPanel.h"
#include "Capture.h"
#include "Metrics.h"
#include "OpenGL.h"
#include "QuadDrawing.h"
#include "tiny2d/Input.h"
#include <cctype>
#include <iostream>
#include <cerrno>
#include <poll.h>
#include <unistd.h>

namespace lab {
namespace {
struct ShaderFrameCommand { bool reload; std::filesystem::path output; };

// Poll complete lines so the native window can process events while the editor
// is changing an external file. Only this explicitly requested transport reads stdin.
std::optional<ShaderFrameCommand> readCommand(std::string& pending) {
    auto end=pending.find('\n');
    if (end==std::string::npos) {
        pollfd input{STDIN_FILENO,POLLIN,0};
        const auto ready=::poll(&input,1,0);
        if (!ready || (ready<0 && errno==EINTR)) return {};
        if (ready<0) throw std::system_error(errno,std::generic_category(),"poll shader controls");
        char buffer[1024];
        const auto count=::read(STDIN_FILENO,buffer,sizeof(buffer));
        if (count<0 && errno==EINTR) return {};
        if (count<0) throw std::system_error(errno,std::generic_category(),"read shader controls");
        if (!count) throw std::runtime_error("shader control input ended before the requested frames");
        pending.append(buffer,std::size_t(count));
        end=pending.find('\n');
        if (end==std::string::npos && pending.size()<=4096) return {};
    }
    if (end>4096) throw std::invalid_argument("shader control line exceeds 4096 bytes");
    auto line=pending.substr(0,end);
    pending.erase(0,end+1);
    if (!line.empty() && line.back()=='\r') line.pop_back();
    const auto space=line.find(' ');
    if (space==line.npos || space+1>=line.size() || line.find('\0')!=line.npos)
        throw std::invalid_argument("shader command expects draw or reload followed by a new PNG path");
    const auto verb=line.substr(0,space);
    if (verb!="draw" && verb!="reload") throw std::invalid_argument("unknown shader control command");
    ShaderFrameCommand cmd{verb=="reload",line.substr(space+1)};
    tiny2d::requireExternalOutput(cmd.output);
    return cmd;
}

void galleryHud(Picture& picture, const FrameInfo& info) {
    picture.hud.clear();
    if (!info.display.hud) return;
    const auto size=info.logical;
    picture.hudCamera=tiny2d::Camera2D(size.width,size.height);
    picture.hudCamera.setPosition(size.width*.5f,size.height*.5f);
    std::string caption=std::string(galleryName(*info.galleryEffect))+"   /   "+name(info.display.view);
    for (auto& ch : caption) ch=static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    quad::rect(picture.hud,8,8,16+6*caption.size(),22,{.025f,.038f,.052f,.93f});
    quad::lettering(picture.hud,caption,16,15,1,{.88f,.95f,.98f,1});
}
}

void runShaders(Options o) {
    const bool finite=o.frames.has_value();
    tiny2d::Window window(o.width,o.height,"Tiny2D Lab / Shader Gallery");
    tiny2d::Input input(window);
    if (finite) glfwSwapInterval(0);
    std::cout<<"opengl="<<glGetString(GL_VERSION)<<" renderer="<<glGetString(GL_RENDERER)<<'\n';
    Pipeline pipe;
    std::filesystem::path workspace;
    std::string workspaceError;
    const auto defaults=assetPath("shaders/gallery/aurora.frag").parent_path();
    if (!o.shaderWorkspace.empty()) workspace=prepareShaderWorkspace(o.shaderWorkspace,defaults);
    else if (tiny2d::insideApplication(tiny2dExecutablePath())) {
        try { workspace=prepareShaderWorkspace(defaultShaderWorkspace(),defaults); }
        catch (const std::exception& error) {
            workspaceError="Editable workspace unavailable. Bundled shaders active. "+std::string(error.what());
            std::cerr<<workspaceError<<'\n';
        }
    }
    ShaderGallery gallery(workspace);
    if (!workspace.empty()) std::cout<<"shader_workspace="<<workspace.string()<<'\n';
    std::string pending;
    if (o.shaderControl) std::cout<<"shader_control_ready version="<<gallery.shader(o.effect).status().version<<std::endl;
    Controls controls;
    ToolState tools{o.inspector,o.timing,o.previews};
    std::unique_ptr<Inspector> gui;
    if (!finite || tools.visible) gui=std::make_unique<Inspector>(window,finite);
    auto queries=o.timing ? tiny2d::openTimestampQueries(Timer::capacity) : nullptr;
    std::unique_ptr<Timer> timer;
    if (queries) timer=std::make_unique<Timer>(*queries);

    Picture picture;
    Metrics csv(o.metrics);
    std::vector<CpuSample> samples;
    if (finite) samples.reserve(1200);
    std::uint64_t frames=0, tick=0, epoch=1;
    auto previous=Clock::now();
    Clock::duration waited{};
    std::optional<Clock::time_point> suspended;
    std::string resizeError;
    while (!finite || frames<*o.frames) {
        csv.flushRetired();
        window.pollEvents();
        if (window.shouldClose()) {
            if (finite) throw std::runtime_error("window closed before requested shader frames");
            break;
        }
        const auto start=Clock::now();
        const double dt=std::chrono::duration<double>(start-previous).count();
        previous=start;
        CpuSample cpu;
        auto& info=cpu.info;
        info.id=frames+1;
        info.measurement=info.id>120 && info.id<=1320;
        info.logical=window.size();
        info.pixels=window.framebufferSize();
        bool drawable=false;
        if (info.logical.width>0 && info.logical.height>0 && !window.iconified()) {
            try { drawable=pipe.resize(info.pixels); resizeError.clear(); }
            catch (const std::exception& error) {
                if (finite) throw;
                if (resizeError!=error.what()) std::cerr<<error.what()<<'\n';
                resizeError=error.what();
            }
        }
        if (finite && suspended && waited+(start-*suspended)>=std::chrono::seconds(5))
            throw std::runtime_error("drawable wait exceeded 5 cumulative seconds");
        if (!drawable) {
            if (!suspended) suspended=start;
            controls.poll({},false,0,o.display);
            if (gui) gui->suspend();
            glfwWaitEventsTimeout(.05);
            continue;
        }
        if (suspended) { waited+=start-*suspended; suspended.reset(); }

        std::optional<ShaderFrameCommand> frameCommand;
        if (o.shaderControl) {
            frameCommand=readCommand(pending);
            if (!frameCommand) { glfwWaitEventsTimeout(.015); continue; }
        }
        if (timer && timer->hasOutstanding()) {
            const auto poll=Clock::now();
            timer->poll([&](FrameInfo origin,tiny2d::QueryStatus status,const Timer::Durations& times) {
                csv.retired({origin,info.id,status,times});
            });
            cpu.poll=micros(poll);
        }
        const bool focused=input.focused();
        if (!finite && window.takeFocusLoss()) controls.poll({},false,0,o.display);
        RoutedInput routed;
        if (gui) {
            const auto build=Clock::now();
            routed=gui->buildPanel(tools,focused,[&](Commands& cmd) {
                shaderPanel(tools,cmd,o.effect,gallery.shader(o.effect),tick,controls.paused,o.display,finite,workspaceError);
            },true);
            cpu.build=micros(build);
        }
        const auto update=Clock::now();
        bool reload=frameCommand && frameCommand->reload;
        if (finite) {
            if (!frameCommand) ++tick;
        } else {
            for (auto key : {ToggleLighting,ToggleNormal,ToggleShadows,ShowLight,ShowNormal,ShowShadow})
                routed.keys.reset(key);
            const auto action=controls.poll(routed.keys,focused,dt,o.display,routed.captureKeyboard,
                                            routed.commands,routed.freshFocus);
            if (action.exit) break;
            if (action.reset) { ++epoch; tick=0; }
            tick+=action.steps;
            reload=action.pressed[ReloadShader];
        }
        if (reload) {
            auto& shader=gallery.shader(o.effect);
            if (!shader.reload()) std::cerr<<shader.status().error<<'\n';
        }
        cpu.update=micros(update);
        info.tick=tick; info.epoch=epoch; info.seed=o.seed;
        info.generation=pipe.generation(); info.display=o.display;
        info.galleryEffect=o.effect; info.shaderVersion=gallery.shader(o.effect).status().version;
        info.inspector=tools.visible; info.timing=o.timing;
        cpu.hdrBytes=pipe.storage();
        const auto prepare=Clock::now();
        galleryHud(picture,info);
        cpu.prepare=micros(prepare);
        DrawTools draw;
        draw.gallery=&gallery.shader(o.effect);
        draw.tick=tick;
        if (gui) {
            const auto build=Clock::now();
            try {
                draw.previews=gui->finish(tools,pipe,info,picture);
                if (tools.visible) draw.gui=[&](Stats& s){gui->submit(s);};
            } catch (const PreviewAllocationError& error) {
                if (finite) throw;
                tools.previews=false;
                std::cerr<<"Buffer previews disabled: "<<error.what()<<'\n';
            }
            cpu.build+=micros(build);
        }
        info.previews=draw.previews;
        if (timer) {
            info.timestampBits=queries->bits;
            cpu.status=timer->begin(info);
            if (cpu.status==tiny2d::QueryStatus::Pending) draw.timer=timer.get();
        }
        try {
            if (draw.timer) draw.timer->mark(0,false);
            pipe.draw(picture,o.display,cpu.stats,draw);
            if (draw.timer) draw.timer->mark(0,true);
            info.mask=cpu.stats.mask;
            if (gui) cpu.stats.fontBytes=gui->atlasBytes();
            cpu.previewBytes=pipe.previewBytes();
            cpu.validGpuSamples=timer ? timer->counts.completed : 0;
            cpu.total=micros(start);
            if (frameCommand) captureNew(frameCommand->output.string(),info.pixels);
            if (finite && info.id==*o.frames && !o.capture.empty()) capture(o.capture,info.pixels);
            window.swapBuffers();
            const char* error=nullptr;
            if (glfwGetError(&error)!=GLFW_NO_ERROR)
                throw std::runtime_error(std::string("window submission: ")+(error?error:"unknown error"));
        } catch (...) {
            if (draw.timer) {
                info.mask=cpu.stats.mask;
                draw.timer->finish(info,false);
                csv.retired({info,info.id,tiny2d::QueryStatus::Invalid,{}});
            }
            throw;
        }
        if (draw.timer) draw.timer->finish(info,true);
        csv.record(cpu);
        if (finite && info.measurement) samples.push_back(cpu);
        ++frames;
        if (frameCommand) {
            const auto& state=gallery.shader(o.effect).status();
            std::cout<<"shader_control frame="<<frames<<" tick="<<tick<<" version="<<state.version
                     <<" result="<<(state.success?"ok":"failed")<<std::endl;
        }
    }
    csv.close();
    report(samples);
    std::cout<<"status=ok scene=shaders frames="<<frames<<" ticks="<<tick<<" effect="<<galleryName(o.effect)
             <<" shader_version="<<gallery.shader(o.effect).status().version
             <<" timestamp_bits="<<(queries?queries->bits:-1)
             <<" valid_gpu_samples="<<(timer?timer->counts.completed:0)<<'\n';
}
}
