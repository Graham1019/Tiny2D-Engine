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
    std::uint64_t frames=0, storage=0, shadowStorage=0, materialUpload=0;
    tiny2d::Size logical{}, pixels{};
    // Keep the Context alive while reporting and retiring queries on every exit path.
    std::optional<tiny2d::Window> owner;
    std::unique_ptr<tiny2d::TimestampQueries> queries;
    std::unique_ptr<Timer> timer;
    tiny2d::QueryCounts measurement;
    auto summary=[&](const char* status) {
        const auto counts=timer ? timer->counts : tiny2d::QueryCounts{};
        std::cout<<"gpu_requested="<<counts.requested<<" completed="<<counts.completed<<" pending="<<counts.pending
                 <<" skipped="<<counts.skipped<<" invalid="<<counts.invalid<<" reason="
                 <<(queries?queries->reason:(o.timing?"not initialized":"timing off"))<<'\n';
        if (o.frames) std::cout<<"measurement_gpu_requested="<<measurement.requested
            <<" completed="<<measurement.completed<<" pending="<<measurement.pending
            <<" skipped="<<measurement.skipped<<" invalid="<<measurement.invalid<<'\n';
        std::cout<<"status="<<status<<" frames="<<frames<<" ticks="<<neon.tick<<" seed="<<o.seed
                 <<" window="<<logical.width<<'x'<<logical.height<<" framebuffer="<<pixels.width<<'x'<<pixels.height
                 <<" view="<<name(o.display.view)<<" bloom="<<(o.display.bloom?"on":"off")
                 <<" strength="<<o.display.strength/10.f<<" last_passes="<<last.passes
                 <<" fullscreen="<<last.fullscreen<<" draws="<<last.draws<<" sprites="<<last.sprites
                 <<" upload_bytes="<<(last.bytes+last.vertexBytes+last.indexBytes+last.fontUpload+last.lightUniformBytes+materialUpload+last.shadowVertexBytes)
                 <<" lighting="<<(o.display.lighting?"on":"off")<<" active_lights="<<picture.lights.size()
                 <<" light_uniform_bytes="<<last.lightUniformBytes
                 <<" normals="<<(o.display.normalMapping?"on":"off")<<" material_upload_bytes="<<materialUpload
                 <<" shadows="<<(o.display.shadows?"on":"off")<<" shadow_bytes="<<shadowStorage
                 <<" shadow_casters="<<last.activeCasters<<" shadow_skipped="<<last.skippedCasters
                 <<" shadow_vertex_bytes="<<last.shadowVertexBytes
                 <<" batch_vertex_bytes="<<last.bytes<<" rgba16f_attachment_bytes="<<storage
                 <<" attempt_passes="<<attempt.passes<<" attempt_draws="<<attempt.draws
                 <<" attempt_complete="<<attempt.complete<<'\n';
    };
    try {
        auto& window=owner.emplace(o.width,o.height,"Tiny2D Lab / Neon");
        tiny2d::Input input(window);
        if (o.frames) glfwSwapInterval(0);
        GLint profile;
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK,&profile);
        std::cout<<"profile="<<((profile & GL_CONTEXT_CORE_PROFILE_BIT)?"core":"other")<<" opengl="<<glGetString(GL_VERSION)<<" vendor="<<glGetString(GL_VENDOR)<<" renderer="<<glGetString(GL_RENDERER)<<'\n';
        Pipeline renderer;
        ToolState tools{o.inspector,o.timing,o.previews};
        std::unique_ptr<Inspector> gui;
        if (!o.frames || tools.visible) gui=std::make_unique<Inspector>(window,o.frames.has_value());
        auto enableTimer=[&] {
            if (timer) return;
            queries=tiny2d::openTimestampQueries(Timer::capacity);
            timer=std::make_unique<Timer>(*queries);
            std::cout<<"timestamp="<<queries->reason<<" capacity="<<Timer::capacity<<'\n';
        };
        if (tools.timing) enableTimer();
        Metrics csv(o.metrics);
        History history;
        Presented presented;
        std::uint64_t epoch=1;
        std::vector<CpuSample> samples;
        samples.reserve(1200);
        auto previous=Clock::now();
        Clock::duration waited{};
        std::optional<Clock::time_point> unavailableSince;
        std::string resizeError;
        while (!o.frames || frames<*o.frames) {
            csv.flushRetired(); // Outside both clocks, including delayed warmup records.
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
            const auto priorMaterialBytes=renderer.materialBytes();
            bool drawable=false;
            if (logical.width>0 && logical.height>0 && !window.iconified()) {
                try {
                    drawable=renderer.resize(pixels,o.display.lighting,o.display.normalMapping,o.display.shadows);
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
            if (timer && timer->hasOutstanding()) {
                const auto polling=Clock::now();
                timer->poll([&](FrameInfo f,tiny2d::QueryStatus status,const Timer::Durations& times) {
                    GpuSample sample{f,info.id,status,times};
                    csv.retired(sample);
                    if (f.measurement) {
                        --measurement.pending;
                        if (status==tiny2d::QueryStatus::Complete) ++measurement.completed;
                        else ++measurement.invalid;
                    }
                    history.recordGpu(epoch,sample);
                });
                cpu.poll=micros(polling);
            }
            if (!o.frames && window.takeFocusLoss()) controls.poll({},false,0,o.display);
            RoutedInput routed;
            if (gui) {
                const auto build=Clock::now();
                routed=gui->build(tools,neon,epoch,o.display,controls.paused,history,
                        timer?timer->counts:tiny2d::QueryCounts{},queries?queries->reason:"off",input.focused());
                cpu.build=micros(build);
            }
            const Content content{0,0,logical};
            const auto oldView=o.display.view;
            const bool wasLit=o.display.lighting;
            const bool hadNormals=o.display.normalMapping;
            const bool hadShadows=o.display.shadows;
            Action action;
            if (o.frames) action.steps=1;
            else action=controls.poll(routed.keys,input.focused(),dt,o.display,routed.captureKeyboard,routed.commands,routed.freshFocus);
            if (action.exit) break;
            if (wasLit!=o.display.lighting || hadNormals!=o.display.normalMapping || hadShadows!=o.display.shadows) {
                // Resolve a same-frame GUI/key toggle before borrowing any attachment.
                try { renderer.resize(pixels,o.display.lighting,o.display.normalMapping,o.display.shadows); }
                catch (const std::exception& e) {
                    std::cerr<<"Lighting change failed: "<<e.what()<<'\n';
                    o.display.lighting=wasLit;
                    o.display.normalMapping=hadNormals;
                    o.display.shadows=hadShadows;
                    o.display.view=oldView;
                    renderer.resize(pixels,wasLit,hadNormals,hadShadows);
                }
                info.generation=renderer.generation();
            }
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
            picture.prepare(neon,o.display,controls.paused,logical,pixels,last,epoch);
            ShadowCasters casters{};
            if (o.display.shadows) {
                casters=selectShadowCasters(picture.lights,o.display.shadowLights);
                if (!gui) tools.resolveShadow(picture.lights,casters,epoch);
            }
            Presented next;
            if (tools.visible) {
                next={objects(neon,epoch),picture.worldCamera,content,o.display.view,info.generation,info.id,neon.tick};
                if (tools.selected && !resolve(neon,epoch,*tools.selected)) tools.selected.reset();
                const bool sceneView=o.display.view==View::Final || o.display.view==View::Scene;
                if (sceneView && (tools.bounds || (tools.outline && tools.selected)))
                    picture.bounds(next.objects,tools.selected,tools.bounds,tools.outline);
                if (tools.lightRadii && o.display.lighting && (sceneView || o.display.view==View::Light))
                    picture.lightRadii(tools.selected);
            }
            cpu.lightExtract=picture.lightExtractUs;
            cpu.prepare=micros(prepareStart)-cpu.lightExtract;
            cpu.activeLights=picture.lights.size();
            cpu.droppedLights=picture.lights.dropped();
            cpu.lightSize=renderer.lightSize();
            cpu.normalSize=renderer.normalSize();
            cpu.materialBytes=renderer.materialBytes();
            if (cpu.materialBytes && !priorMaterialBytes) {
                cpu.normalPrepare=renderer.normalPrepareUs();
                cpu.materialUpload=cpu.materialBytes;
            }
            storage=renderer.storage();
            shadowStorage=renderer.shadowBytes();
            cpu.shadowBytes=shadowStorage;
            DrawTools draw;
            if (gui) {
                const auto build=Clock::now();
                try {
                    draw.previews=gui->finish(tools,renderer,info,picture,casters);
                    if (tools.visible) draw.gui=[&](Stats& s){gui->submit(s);};
                } catch (const PreviewAllocationError& e) {
                    if (o.frames) throw;
                    std::cerr<<"Buffer previews disabled: "<<e.what()<<'\n';
                    tools.previews=false;
                }
                cpu.build+=micros(build);
            }
            if (o.display.shadows) {
                const auto debugStart=Clock::now();
                draw.shadowSource=tools.shadowSource;
                draw.frame=info.id; draw.tick=info.tick; draw.epoch=info.epoch;
                if (o.display.view==View::Shadow)
                    picture.shadowCaption(draw.shadowSource,info.id,info.tick,info.epoch,info.generation);
                if (tools.visible && (tools.occluderEdges || tools.shadowGeometry) &&
                    (o.display.view==View::Final || o.display.view==View::Scene || o.display.view==View::Light)) {
                    const auto* source=draw.shadowSource ? picture.lights.find(*draw.shadowSource) : nullptr;
                    picture.shadowDebug(source,tools.occluderEdges,tools.shadowGeometry);
                }
                cpu.prepare+=micros(debugStart);
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
                if (gui) attempt.fontBytes=gui->atlasBytes();
                cpu.stats=attempt; cpu.hdrBytes=storage; cpu.previewBytes=renderer.previewBytes();
                cpu.total=micros(start);
                if (o.frames && frames+1==*o.frames && !o.capture.empty()) capture(o.capture,pixels);
                window.swapBuffers();
                const char* error=nullptr;
                if (glfwGetError(&error)!=GLFW_NO_ERROR)
                    throw std::runtime_error(std::string("window submission: ")+(error?error:"unknown error"));
            } catch (...) {
                info.mask=attempt.mask;
                if (draw.timer) {
                    draw.timer->finish(info,false);
                    csv.retired({info,info.id,tiny2d::QueryStatus::Invalid,{}});
                    if (info.measurement) { --measurement.pending; ++measurement.invalid; }
                }
                if (gui) gui->discard();

                throw;
            }
            ++frames;
            if (draw.timer) draw.timer->finish(info,true);
            presented=std::move(next);
            last=attempt;
            materialUpload=cpu.materialUpload;
            History::append(history.cpu,cpu);
            csv.record(cpu);
            if (o.frames && info.measurement) samples.push_back(cpu);
        }
        report(samples);
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
        std::cout<<"tiny2d_lab [--scene neon] [--seed N] [--frames N] [--size WxH] [--view scene|bright|blur|final|light|normal|shadow] "
                 <<"[--lighting on|off] [--ambient 0..2] "
                 <<"[--normal-mapping on|off] [--normal-material flat|tile] [--normal-strength 0..2] "
                 <<"[--shadows on|off] [--shadow-lights 1..8] [--occluders none|demo] "
                 <<"[--bloom on|off] [--capture PNG] [--inspector on|off] [--gpu-timing on|off] "
                 <<"[--previews off|all] [--metrics CSV]\nInteractive: Inspector and GPU timing on. Finite frames: both off. Previews default off.\n";
        return 0;
    }
    try { lab::run(lab::parse(argc,argv)); }
    catch (const std::exception& e) { std::cerr<<"Tiny2D Lab: "<<e.what()<<'\n'; return 1; }
}
