#include "ParticleApp.h"
#include "ParticlePresentation.h"
#include "Capture.h"
#include "Metrics.h"
#include "OpenGL.h"
#include "tiny2d/Input.h"
#include <iostream>

namespace lab {
void runParticles(Options o) {
    const bool finite=o.frames.has_value();
    tiny2d::Window window(o.width,o.height,"Tiny2D Lab / Particle Forcefield");
    tiny2d::Input input(window);
    if (finite) glfwSwapInterval(0);
    std::cout<<"opengl="<<glGetString(GL_VERSION)<<" vendor="<<glGetString(GL_VENDOR)
             <<" renderer="<<glGetString(GL_RENDERER)<<'\n';
    Pipeline pipe;
    ToolState tools{o.inspector,o.timing,o.previews};
    std::unique_ptr<Inspector> gui;
    if (!finite || tools.visible) gui=std::make_unique<Inspector>(window,finite);
    std::unique_ptr<tiny2d::TimestampQueries> queries;
    std::unique_ptr<Timer> timer;
    auto enableTimer=[&] {
        if (timer) return;
        queries=tiny2d::openTimestampQueries(Timer::capacity);
        timer=std::make_unique<Timer>(*queries);
        std::cout<<"timestamp="<<queries->reason<<" bits="<<queries->bits<<" capacity="<<Timer::capacity<<'\n';
    };
    if (tools.timing) enableTimer();

    ParticleSimulation sim(o.seed,o.particleCapacity,o.particleRate,o.particleSteady);
    ParticleRenderSnapshot snapshot(sim.capacity());
    Controls controls;
    ParticleEditor edit;
    Picture picture;
    Metrics csv(o.metrics);
    History history;
    std::vector<CpuSample> samples;
    if (finite) samples.reserve(1200);
    std::uint64_t frames=0;
    auto previous=Clock::now();
    Clock::duration waited{};
    std::optional<Clock::time_point> unavailableSince;
    std::string resizeError;
    tiny2d::Size logical{},pixels{};
    while (!finite || frames<*o.frames) {
        csv.flushRetired();
        window.pollEvents();
        if (window.shouldClose()) {
            if (finite) throw std::runtime_error("window closed before requested particle frames");
            break;
        }
        const auto start=Clock::now();
        const double elapsed=std::chrono::duration<double>(start-previous).count();
        previous=start;
        logical=window.size(); pixels=window.framebufferSize();
        bool drawable=false;
        if (logical.width>0 && logical.height>0 && !window.iconified()) {
            try {
                drawable=pipe.resize(pixels,o.display.lighting);
                resizeError.clear();
            } catch (const std::exception& e) {
                if (finite) throw;
                if (resizeError!=e.what()) std::cerr<<"resize: "<<e.what()<<'\n';
                resizeError=e.what();
            }
        }
        if (finite && unavailableSince && waited+start-*unavailableSince>=std::chrono::seconds(5))
            throw std::runtime_error("drawable wait exceeded 5 cumulative seconds");
        if (!drawable) {
            if (!unavailableSince) unavailableSince=start;
            controls.poll({},false,0,o.display);
            edit.dragging=false;
            if (gui) gui->suspend();
            glfwWaitEventsTimeout(.05);
            continue;
        }
        if (unavailableSince) { waited+=start-*unavailableSince; unavailableSince.reset(); }

        CpuSample cpu;
        auto& info=cpu.info;
        info.id=frames+1; info.logical=logical; info.pixels=pixels;
        info.particles=true; info.demoScript=o.particleDemo; info.seed=o.seed;
        info.particleRate=o.particleRate; info.particleCapacity=o.particleCapacity; info.particleSteady=o.particleSteady;
        info.measurement=info.id>120 && info.id<=1320;
        if (timer && timer->hasOutstanding()) {
            const auto polling=Clock::now();
            timer->poll([&](FrameInfo f,tiny2d::QueryStatus status,const Timer::Durations& times) {
                GpuSample sample{f,info.id,status,times};
                csv.retired(sample);
                history.recordGpu(sim.epoch(),sample);
            });
            cpu.poll=micros(polling);
        }
        const bool focused=input.focused();
        if (!finite && window.takeFocusLoss()) {
            controls.poll({},false,0,o.display);
            edit.dragging=false;
        }
        RoutedInput routed;
        if (gui) {
            const auto build=Clock::now();
            routed=gui->buildPanel(tools,focused,[&](Commands& cmd) {
                particlePanel(tools,edit,cmd,sim,o,controls.paused,history,queries?queries->reason:"off");
            });
            cpu.build=micros(build);
        }
        Action action;
        if (finite) action.steps=1;
        else {
            const auto before=o.display;
            action=controls.poll(routed.keys,focused,elapsed,o.display,routed.captureKeyboard,routed.commands,routed.freshFocus);
            if (action.exit) break;
            if (action.pressed.any()) edit.notice.clear();
            if (action.pressed[ShowNormal] || action.pressed[ShowShadow] ||
                action.pressed[ToggleNormal] || action.pressed[ToggleShadows]) {
                edit.notice="NORMAL / SHADOW N/A  NO MATERIAL OR CASTERS";
                o.display.normalMapping=o.display.shadows=false;
                if (o.display.view==View::Normal || o.display.view==View::Shadow) o.display.view=before.view;
            }
            if (action.pressed[ShowLight] && !o.display.lighting) edit.notice="LIGHT VIEW REQUIRES LIGHTING ON";
            if (before.lighting!=o.display.lighting) {
                try { pipe.resize(pixels,o.display.lighting); }
                catch (const std::exception& e) {
                    o.display=before;
                    pipe.resize(pixels,before.lighting);
                    std::cerr<<"Lighting change failed: "<<e.what()<<'\n';
                }
            }
            if (action.reset) {
                sim.reset(); edit.pending={}; edit.dragging=false; history={};
            } else if (!o.particleDemo) {
                edit.pointer(routed,sim,particleCamera(logical),logical,o.display.view);
                if (action.pressed[Fire] && !o.particleSteady)
                    edit.pending.burst=std::min(ParticleSimulation::maxBurst,edit.pending.burst+128u);
            }
        }
        const auto updateStart=Clock::now();
        for (unsigned i=0;i<action.steps;++i)
            cpu.forceUpdateUs+=sim.step(o.particleDemo ? particleDemoTick(sim) : edit.takeTick(sim,action.motion));
        cpu.update=micros(updateStart);
        info.epoch=sim.epoch(); info.tick=sim.tick(); info.generation=pipe.generation(); info.display=o.display;
        info.inspector=tools.visible; info.timing=tools.timing;
        cpu.activeParticles=sim.active(); cpu.droppedParticles=sim.dropped(); cpu.activeFields=sim.activeFields();
        const auto prepare=Clock::now();
        sim.snapshot(snapshot);
        cpu.snapshotPrepareUs=micros(prepare);
        prepareParticles(picture,sim,edit,o,controls.paused,logical);
        cpu.prepare=micros(prepare);
        cpu.lightSize=pipe.lightSize(); cpu.hdrBytes=pipe.storage();
        DrawTools draw;
        draw.particles=&snapshot;
        if (gui) {
            const auto build=Clock::now();
            try {
                draw.previews=gui->finish(tools,pipe,info,picture);
                if (tools.visible) draw.gui=[&](Stats& s){gui->submit(s);};
            } catch (const PreviewAllocationError& e) {
                if (finite) throw;
                tools.previews=false;
                std::cerr<<"Buffer previews disabled: "<<e.what()<<'\n';
            }
            cpu.build+=micros(build);
        }
        info.previews=draw.previews;
        if (tools.timing) {
            enableTimer();
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
            if (finite && info.id==*o.frames && !o.capture.empty()) capture(o.capture,pixels);
            window.swapBuffers();
            const char* error=nullptr;
            if (glfwGetError(&error)!=GLFW_NO_ERROR)
                throw std::runtime_error(std::string("window submission: ")+(error?error:"unknown error"));
        } catch (...) {
            info.mask=cpu.stats.mask;
            if (draw.timer) {
                draw.timer->finish(info,false);
                csv.retired({info,info.id,tiny2d::QueryStatus::Invalid,{}});
            }
            if (gui) gui->discard();
            throw;
        }
        if (draw.timer) draw.timer->finish(info,true);
        ++frames;
        History::append(history.cpu,cpu);
        csv.record(cpu);
        if (finite && info.measurement) samples.push_back(cpu);
    }
    report(samples);
    csv.close();
    const auto counts=timer ? timer->counts : tiny2d::QueryCounts{};
    const auto last=history.cpu.empty()?Stats{}:history.cpu.back().stats;
    std::cout<<"status=ok scene=particles frames="<<frames<<" ticks="<<sim.tick()<<" seed="<<o.seed
             <<" script="<<(o.particleDemo?"demo":"none")<<" capacity="<<sim.capacity()<<" active="<<sim.active()
             <<" dropped="<<sim.dropped()<<" active_fields="<<sim.activeFields()
             <<" window="<<logical.width<<'x'<<logical.height<<" framebuffer="<<pixels.width<<'x'<<pixels.height
             <<" particle_draws="<<last.particleDrawCalls<<" draws="<<last.draws
             <<" instance_upload_bytes="<<last.instanceUploadBytes<<" timestamp_bits="<<(queries?queries->bits:-1)
             <<" valid_gpu_samples="<<counts.completed<<" gpu_pending="<<counts.pending<<" gpu_invalid="<<counts.invalid<<'\n';
}
}
