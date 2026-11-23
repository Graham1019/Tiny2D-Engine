#include "Inspector.h"
#include "Metrics.h"
#include "OpenGL.h"
#include "tiny2d/Input.h"
#include "Capture.h"
#include <algorithm>
#include <cmath>
#include <iostream>


namespace lab {
namespace {
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
        std::cout<<"profile="<<((profile & GL_CONTEXT_CORE_PROFILE_BIT)?"core":"other")<<" opengl="<<glGetString(GL_VERSION)<<" vendor="<<glGetString(GL_VENDOR)<<" renderer="<<glGetString(GL_RENDERER)<<'\n';
        Pipeline renderer;
        ToolState tools{o.inspector,o.timing,o.previews};
        std::unique_ptr<Inspector> gui;
        if (!o.frames || tools.visible) gui=std::make_unique<Inspector>(window,o.frames.has_value());
        std::unique_ptr<tiny2d::TimestampQueries> queries;
        std::unique_ptr<Timer> timer;
        auto enableTimer=[&] {
            if (timer) return;
            queries=tiny2d::openTimestampQueries(Timer::capacity);
            timer=std::make_unique<Timer>(*queries);
            std::cout<<"timestamp="<<queries->reason<<" capacity="<<Timer::capacity<<'\n';
        };
        if (tools.timing) enableTimer();
        Metrics csv(o.metrics);
        History history;
        tiny2d::QueryCounts measurement;
        Presented presented;
        std::uint64_t epoch=1;
        std::vector<CpuSample> samples;
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
                if (gui) gui->suspend();
                presented={};
                glfwWaitEventsTimeout(.05);
                continue;
            }
            if (unavailableSince) {
                waited+=start-*unavailableSince;
                unavailableSince.reset();
            }
            CpuSample cpu;
            auto& info=cpu.info;
            info.id=frames+1; info.generation=renderer.generation(); info.logical=logical; info.pixels=pixels;
            info.measurement=info.id>120 && info.id<=1320;
            std::vector<GpuSample> arrived;
            if (timer && timer->hasOutstanding()) {
                const auto polling=Clock::now();
                timer->poll([&](FrameInfo f,tiny2d::QueryStatus status,const Timer::Durations& times) {
                    GpuSample sample{f,info.id,status,times};
                    arrived.push_back(sample);
                    if (f.measurement) {
                        --measurement.pending;
                        if (status==tiny2d::QueryStatus::Complete) ++measurement.completed;
                        else ++measurement.invalid;
                    }
                    if (f.epoch==epoch && status==tiny2d::QueryStatus::Complete)
                        History::append(history.gpu,sample);
                });
                cpu.poll=micros(polling);
            }
            if (!o.frames && window.takeFocusLoss()) {
                controls.poll({},false,0,o.display);
                gui->suspend();
            }
            RoutedInput routed;
            if (gui) {
                const auto build=Clock::now();
                routed=gui->build(tools,neon,epoch,o.display,controls.paused,history,
                        timer?timer->counts:tiny2d::QueryCounts{},queries?queries->reason:"off",input.focused());
                cpu.build=micros(build);
            }
            const Content content{0,0,logical};
            const auto oldView=o.display.view;
            Action action;
            if (o.frames) action.steps=1;
            else action=controls.poll(routed.keys,input.focused(),dt,o.display,routed.captureKeyboard,routed.commands);
            if (action.exit) break;
            if (action.reset) {
                ++epoch; neon=Neon(o.seed); presented={}; tools.selected.reset(); history={}; last={};
            } else if (tools.visible && routed.click && oldView==o.display.view &&
                       (oldView==View::Final || oldView==View::Scene)) {
                tools.selected=presented.pick(*routed.click,content,o.display.view,info.generation,neon,epoch);
            }
            const auto updateStart=Clock::now();
            for (unsigned i=0;i<action.steps;++i) neon.update(action.motion);
            cpu.update=micros(updateStart);
            info.tick=neon.tick; info.epoch=epoch; info.display=o.display;
            info.inspector=tools.visible; info.timing=tools.timing;
            const auto prepareStart=Clock::now();
            picture.prepare(neon,o.display,controls.paused,logical,pixels,last);
            Presented next;
            if (tools.visible) {
                next={objects(neon,epoch),picture.worldCamera,content,o.display.view,info.generation,info.id,neon.tick};
                if (tools.selected && !resolve(neon,epoch,*tools.selected)) tools.selected.reset();
                if (tools.bounds || (tools.outline && tools.selected))
                    picture.bounds(next.objects,tools.selected,tools.bounds,tools.outline);
            }
            cpu.prepare=micros(prepareStart);
            storage=renderer.storage();
            DrawTools draw;
            if (gui) {
                const auto build=Clock::now();
                draw.previews=gui->finish(tools,renderer,info.id,info.tick);
                cpu.build+=micros(build);
                if (tools.visible) draw.gui=[&](Stats& s){gui->submit(s);};
            }
            info.previews=draw.previews;
            if (tools.timing) {
                enableTimer();
                info.timestampBits=queries->bits;
                cpu.status=timer->begin(info);
                if (info.measurement && queries->bits) {
                    ++measurement.requested;
                    if (cpu.status==tiny2d::QueryStatus::Pending) ++measurement.pending;
                    else ++measurement.skipped;
                }
                if (cpu.status==tiny2d::QueryStatus::Pending) draw.timer=timer.get();
            }
            try {
                if (draw.timer) draw.timer->mark(0,false);
                renderer.draw(picture,o.display,attempt,draw);
                info.mask=attempt.mask;
                if (draw.timer) draw.timer->mark(0,true);
                cpu.stats=attempt; cpu.hdrBytes=storage; cpu.previewBytes=renderer.previewBytes();
                cpu.total=micros(start);
                if (o.frames && frames+1==*o.frames && !o.capture.empty()) capture(o.capture,pixels);
                window.swapBuffers();
                const char* error=nullptr;
                if (glfwGetError(&error)!=GLFW_NO_ERROR)
                    throw std::runtime_error(std::string("window submission: ")+(error?error:"unknown error"));
            } catch (...) {
                info.mask=attempt.mask;
                if (draw.timer) draw.timer->finish(info,false);
                if (gui) gui->discard();
                if (draw.timer) std::cerr<<"GPU partial frame "<<info.id<<" invalid="<<timer->counts.invalid<<'\n';
                throw;
            }
            ++frames;
            if (draw.timer) draw.timer->finish(info,true);
            presented=std::move(next);
            last=attempt;
            History::append(history.cpu,cpu);
            csv.record(cpu);
            for (const auto& result:arrived) csv.record(result);
            if (o.frames && info.measurement) samples.push_back(cpu);
        }
        report(samples);
        const auto counts=timer ? timer->counts : tiny2d::QueryCounts{};
        std::cout<<"gpu_requested="<<counts.requested<<" completed="<<counts.completed<<" pending="<<counts.pending
                 <<" skipped="<<counts.skipped<<" invalid="<<counts.invalid<<" reason="
                 <<(queries?queries->reason:"timing off")<<'\n';
        if (o.frames) std::cout<<"measurement_gpu_requested="<<measurement.requested
            <<" completed="<<measurement.completed<<" pending="<<measurement.pending
            <<" skipped="<<measurement.skipped<<" invalid="<<measurement.invalid<<'\n';
        csv.close();
    } catch (...) {
        summary("error");
        throw;
    }
    summary("ok");
}
}
}
int main(int argc,char** argv) {
    if (argc==2 && std::string_view(argv[1])=="--help") {
        std::cout<<"tiny2d_lab [--scene neon] [--seed N] [--frames N] [--size WxH] [--view scene|bright|blur|final] "
                 <<"[--bloom on|off] [--capture PNG] [--inspector on|off] [--gpu-timing on|off] "
                 <<"[--previews off|all] [--metrics CSV]\nInteractive: Inspector and GPU timing on. Finite frames: both off. Previews default off.\n";
        return 0;
    }
    try { lab::run(lab::parse(argc,argv)); }
    catch (const std::exception& e) { std::cerr<<"Tiny2D Lab: "<<e.what()<<'\n'; return 1; }
}
