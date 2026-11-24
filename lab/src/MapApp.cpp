#include "MapApp.h"
#include "MapInspector.h"
#include "MapPresentation.h"
#include "NavigationApp.h"
#include "Capture.h"
#include "Metrics.h"
#include "OpenGL.h"
#include "tiny2d/Input.h"
#include <iostream>

namespace lab {
namespace {
bool canPick(const FrameInfo& shown,tiny2d::Size logical,std::uint64_t generation,View view,
             tiny2d::Vector2 cursor) {
    return (view==View::Scene || view==View::Final) && shown.display.view==view &&
           shown.generation==generation && shown.logical.width==logical.width &&
           shown.logical.height==logical.height && cursor.x>=0 && cursor.x<logical.width &&
           cursor.y>=0 && cursor.y<logical.height;
}
}

void runTileLab(Options options) {
    const bool finite=options.frames.has_value();
    const bool navigation=options.scene==LabScene::Navigation;
    tiny2d::Window window(options.width,options.height,navigation?"Tiny2D Lab / Enemy Navigation":"Tiny2D Lab / Seeded Tilemap");
    tiny2d::Input input(window);
    if (finite) glfwSwapInterval(0);
    std::cout<<"opengl="<<glGetString(GL_VERSION)<<" renderer="<<glGetString(GL_RENDERER)<<'\n';
    Pipeline pipeline;
    ToolState tools{options.inspector,options.timing,options.previews};
    std::unique_ptr<Inspector> gui;
    if (!finite || tools.visible || navigation) gui=std::make_unique<Inspector>(window,finite);
    std::unique_ptr<tiny2d::TimestampQueries> queries;
    std::unique_ptr<Timer> timer;
    auto enableTimer=[&] {
        if (timer) return;
        queries=tiny2d::openTimestampQueries(Timer::capacity);
        timer=std::make_unique<Timer>(*queries);
        std::cout<<"timestamp="<<queries->reason<<" bits="<<queries->bits<<'\n';
    };
    if (tools.timing) enableTimer();

    tiny2d::MapParameters parameters;
    parameters.width=options.mapWidth;
    parameters.height=options.mapHeight;
    std::optional<tiny2d::MapSession> mapScene;
    std::optional<NavigationApp> nav;
    if (navigation) nav.emplace(options,parameters);
    else mapScene.emplace(options.seed,parameters);
    const auto& session=nav ? nav->session.world() : *mapScene;
    MapEditor edit{options.seed};
    Controls controls;
    Picture picture;
    History history;
    Metrics csv(options.metrics);
    std::vector<CpuSample> samples;
    if (finite) samples.reserve(1200);
    std::uint64_t frames=0;
    auto previous=Clock::now();
    Clock::duration waited{};
    std::optional<Clock::time_point> suspended;
    std::string resizeError;
    while (!finite || frames<*options.frames) {
        csv.flushRetired();
        window.pollEvents();
        if (window.shouldClose()) {
            if (finite) throw std::runtime_error("window closed before requested map frames");
            break;
        }
        const auto start=Clock::now();
        const double elapsed=std::chrono::duration<double>(start-previous).count();
        previous=start;
        CpuSample cpu;
        auto& info=cpu.info;
        info.id=frames+1;
        info.measurement=info.id>120 && info.id<=1320;
        info.logical=window.size();
        info.pixels=window.framebufferSize();
        bool drawable=false;
        if (info.logical.width>0 && info.logical.height>0 && !window.iconified()) {
            try { drawable=pipeline.resize(info.pixels); resizeError.clear(); }
            catch (const std::exception& error) {
                if (finite) throw;
                if (resizeError!=error.what()) std::cerr<<"resize: "<<error.what()<<'\n';
                resizeError=error.what();
            }
        }
        if (finite && suspended && waited+(start-*suspended)>=std::chrono::seconds(5))
            throw std::runtime_error("drawable wait exceeded 5 cumulative seconds");
        if (!drawable) {
            if (!suspended) suspended=start;
            controls.poll({},false,0,options.display);
            if (gui) gui->suspend();
            glfwWaitEventsTimeout(.05);
            continue;
        }
        if (suspended) { waited+=start-*suspended; suspended.reset(); }

        if (timer && timer->hasOutstanding()) {
            const auto polling=Clock::now();
            timer->poll([&](FrameInfo origin,tiny2d::QueryStatus status,const Timer::Durations& times) {
                GpuSample sample{origin,info.id,status,times};
                csv.retired(sample);
                history.recordGpu(session.epoch(),sample);
            });
            cpu.poll=micros(polling);
        }
        const bool focused=input.focused();
        if (!finite && window.takeFocusLoss()) controls.poll({},false,0,options.display);
        RoutedInput routed;
        edit.regenerate=false;
        if (gui) {
            const auto begin=Clock::now();
            routed=gui->buildPanel(tools,focused,[&](Commands& cmd) {
                if (nav) navigationPanel(tools,edit,nav->view,cmd,nav->session,finite,history,queries.get());
                else mapPanel(tools,edit,cmd,session,options.display,finite,history,queries.get());
            },false,nav && nav->session.upgrades().offer().has_value());
            cpu.build=micros(begin);
        }

        Action action;
        const auto originalEpoch=session.epoch();
        if (finite) action.steps=1;
        else {
            for (auto key:{ToggleLighting,ToggleNormal,ToggleShadows,ShowLight,ShowNormal,ShowShadow})
                routed.keys.reset(key);
            if (edit.regenerate) routed.commands.edges.set(Reset); // Flush the fixed-step accumulator on replacement.
            action=controls.poll(routed.keys,focused,nav && !nav->playing()?0.:elapsed,options.display,routed.captureKeyboard,
                                 routed.commands,routed.freshFocus);
            if (action.exit) break;
            if (nav) nav->session.setPaused(controls.paused);
            else mapScene->setPaused(controls.paused);
            if (action.reset) {
                try {
                    if (nav) {
                        if (edit.regenerate) nav->session.regenerate(edit.seedInput);
                        else nav->session.reset();
                        nav->view.selected=0;
                    } else if (edit.regenerate) mapScene->regenerate(edit.seedInput);
                    else mapScene->reset();
                    edit.selected.reset();
                    edit.seedInput=session.map().seed();
                    edit.error.clear();
                    history={};
                } catch (const std::exception& error) { edit.error=error.what(); }
            } else if (!nav && tools.visible && routed.click && !history.cpu.empty() &&
                       canPick(history.cpu.back().info,info.logical,pipeline.generation(),options.display.view,*routed.click)) {
                const auto cell=session.map().worldToTile(picture.worldCamera.viewportToWorld(*routed.click));
                edit.selected=session.map().inBounds(cell.x,cell.y) ? std::optional{cell} : std::nullopt;
            }
        }

        if (!frames || originalEpoch!=session.epoch()) {
            if (nav) nav->recordInitialBuild(cpu);
            cpu.map.generationUs=session.generationUs();
            cpu.map.validationUs=session.validationUs();
        }
        if (nav) nav->advance(action,options,controls,cpu);
        else for (unsigned i=0;i<action.steps;++i) {
            const auto motion=options.mapWalk ? tiny2d::mapWalkInput(session.tick())
                                               : tiny2d::MapInput{action.motion.x,action.motion.y};
            if (session.paused()) mapScene->step(motion);
            else mapScene->fixedUpdate(motion);
            cpu.map.collisionUs+=session.collisionUs();
            cpu.update+=session.fixedUpdateUs();
        }
        const auto& map=session.map();
        info.tick=session.tick();
        info.epoch=session.epoch();
        info.seed=map.seed();
        info.mapWidth=map.width();
        info.mapHeight=map.height();
        info.navigation=navigation;
        info.mapWalk=nav?options.navWalk:options.mapWalk;
        info.generation=pipeline.generation();
        info.display=options.display;
        info.inspector=tools.visible;
        info.timing=tools.timing;
        bool offerWindow=false;
        if (nav && gui) {
            const auto startOffer=Clock::now();
            offerWindow=nav->drawOffer(finite,focused,controls,cpu);
            cpu.build+=micros(startOffer);
        }
        const auto prepare=Clock::now();
        cpu.map.player=session.playerPosition();
        cpu.map.hitX=session.collision().hitX;
        cpu.map.hitY=session.collision().hitY;
        const auto target=finite && options.mapWalk ? mapSurveyPosition(session.tick()) : session.playerPosition();
        picture.worldCamera=mapCamera(map,target,info.logical);
        cpu.map.camera=picture.worldCamera.viewportToWorld({info.logical.width*.5f,info.logical.height*.5f});
        const auto prepareTiles=Clock::now();
        const auto visible=prepareMapTiles(picture,map,info.logical);
        cpu.map.visiblePrepareUs=micros(prepareTiles);
        cpu.map.totalTiles=map.capacity();
        cpu.map.visibleTiles=visible.count();
        prepareMapActors(picture,map,session.playerPosition(),session.playerHalfSize(),visible,nav?nav->view.bounds:edit.bounds,edit.selected);
        if (nav) nav->prepare(picture,visible,options,cpu);
        else prepareMapHud(picture,options.display,info.logical,map.seed(),session.tick(),session.paused());
        cpu.prepare=micros(prepare);
        cpu.hdrBytes=pipeline.storage();
        DrawTools draw;
        if (gui) {
            const auto begin=Clock::now();
            try {
                draw.previews=gui->finish(tools,pipeline,info,picture,{},offerWindow);
                if (tools.visible || offerWindow) draw.gui=[&](Stats& stats) { gui->submit(stats); };
            } catch (const PreviewAllocationError& error) {
                if (finite) throw;
                tools.previews=false;
                std::cerr<<"Buffer previews disabled: "<<error.what()<<'\n';
            }
            cpu.build+=micros(begin);
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
            pipeline.draw(picture,options.display,cpu.stats,draw);
            if (draw.timer) draw.timer->mark(0,true);
            info.mask=cpu.stats.mask;
            cpu.validGpuSamples=timer ? timer->counts.completed : 0;
            if (gui) cpu.stats.fontBytes=gui->atlasBytes();
            cpu.previewBytes=pipeline.previewBytes();
            cpu.total=micros(start);
            if (finite && info.id==*options.frames && !options.capture.empty()) capture(options.capture,info.pixels);
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
            if (gui) gui->discard();
            throw;
        }
        if (draw.timer) draw.timer->finish(info,true);
        History::append(history.cpu,cpu);
        csv.record(cpu);
        if (finite && info.measurement) samples.push_back(cpu);
        ++frames;
    }
    report(samples);
    csv.close();
    std::cout<<"status=ok scene="<<(navigation?"navigation":"map")<<" frames="<<frames<<" ticks="<<session.tick()<<" seed="<<session.map().seed()
             <<" epoch="<<session.epoch()<<" map_size="<<session.map().width()<<'x'<<session.map().height()
             <<" script="<<((nav?options.navWalk:options.mapWalk)?"walk":"none")<<" shortest_path="<<session.connectivity().shortestPathLength;
    if (nav) nav->printStatus();
    std::cout<<" timestamp_bits="<<(queries?queries->bits:-1)<<" valid_gpu_samples="<<(timer?timer->counts.completed:0)<<'\n';
}
}
