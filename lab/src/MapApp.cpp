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
    const bool replay=options.scene==LabScene::Replay;
    const bool navigation=options.scene==LabScene::Navigation || replay;
    tiny2d::Window window(options.width,options.height,replay?"Tiny2D Lab / Snapshot & Replay":navigation?"Tiny2D Lab / Enemy Navigation":"Tiny2D Lab / Seeded Tilemap");
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
    auto world=[&]() -> const tiny2d::MapSession& { return nav ? nav->displayed().world() : *mapScene; };
    MapEditor edit{world().seed()};
    std::uint64_t revision=0;
    Controls controls;
    controls.paused=world().paused();
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
                history.recordGpu(world().epoch(),sample,revision);
            });
            cpu.poll=micros(polling);
        }
        const bool focused=input.focused();
        if (!finite && window.takeFocusLoss()) controls.poll({},false,0,options.display);
        RoutedInput routed;
        edit.regenerate=false;
        ReplayAction replayAction=ReplayAction::None;
        const auto originalEpoch=world().epoch();
        if (gui) {
            const auto begin=Clock::now();
            routed=gui->buildPanel(tools,focused,[&](Commands& cmd) {
                if (nav) {
                    std::function<void()> transport;
                    if (replay) transport=[&]{replayAction=nav->replay->panel(nav->live);};
                    navigationPanel(tools,edit,nav->view,cmd,nav->displayed(),finite,history,queries.get(),
                                    transport,replay && nav->replay->playback());
                } else mapPanel(tools,edit,cmd,world(),options.display,finite,history,queries.get());
            },false,nav && nav->displayed().upgrades().offer().has_value());
            cpu.build=micros(begin);
        }

        Action action;
        const auto heldKeys=routed.keys;
        const bool transportCommand=focused && replayAction!=ReplayAction::None;
        bool replaced=transportCommand && nav->replay->perform(replayAction,nav->live);
        const bool watching=replay && nav->replay->playback();
        if (finite) action.steps=1;
        else {
            for (auto key:{ToggleLighting,ToggleNormal,ToggleShadows,ShowLight,ShowNormal,ShowShadow})
                routed.keys.reset(key);
            if (watching || transportCommand) {
                for (auto key:{Left,Right,Up,Down,Pause,Step,Reset}) {
                    routed.keys.reset(key);
                    routed.commands.edges.reset(key);
                }
                edit.regenerate=false;
            }
            if (edit.regenerate) routed.commands.edges.set(Reset); // Flush the fixed-step accumulator on replacement.
            action=controls.poll(routed.keys,focused,nav && !nav->playing()?0.:elapsed,options.display,routed.captureKeyboard,
                                 routed.commands,routed.freshFocus);
            if (action.exit) break;
            if (nav) {
                if (!watching && !transportCommand && controls.paused!=world().paused())
                    nav->dispatch(tiny2d_game::ReplayCommand::setPaused(controls.paused));
            } else mapScene->setPaused(controls.paused);
            if (action.reset) {
                try {
                    if (nav) {
                        if (edit.regenerate) nav->dispatch(tiny2d_game::ReplayCommand::regenerate(edit.seedInput,world().parameters()));
                        else nav->dispatch(tiny2d_game::ReplayCommand::reset());
                        nav->view.selected=0;
                    } else if (edit.regenerate) mapScene->regenerate(edit.seedInput);
                    else mapScene->reset();
                    edit.selected.reset();
                    edit.seedInput=world().seed();
                    edit.error.clear();
                    history={};
                } catch (const std::exception& error) { edit.error=error.what(); }
            } else if (!nav && tools.visible && routed.click && !history.cpu.empty() &&
                       canPick(history.cpu.back().info,info.logical,pipeline.generation(),options.display.view,*routed.click)) {
                const auto cell=world().map().worldToTile(picture.worldCamera.viewportToWorld(*routed.click));
                edit.selected=world().map().inBounds(cell.x,cell.y) ? std::optional{cell} : std::nullopt;
            }
        }

        auto recordBuild=[&] {
            if (nav) nav->recordInitialBuild(cpu);
            cpu.map.generationUs=world().generationUs();
            cpu.map.validationUs=world().validationUs();
        };
        const auto beforeAdvanceEpoch=world().epoch();
        if (!frames || replaced || originalEpoch!=beforeAdvanceEpoch) recordBuild();
        if (nav) {
            if (!transportCommand) nav->advance(action,options,controls,cpu);
        } else for (unsigned i=0;i<action.steps;++i) {
            const auto motion=options.mapWalk ? tiny2d::mapWalkInput(world().tick())
                                               : tiny2d::MapInput{action.motion.x,action.motion.y};
            if (world().paused()) mapScene->step(motion);
            else mapScene->fixedUpdate(motion);
            cpu.map.collisionUs+=world().collisionUs();
            cpu.update+=world().fixedUpdateUs();
        }
        const auto& session=world();
        replaced=replaced || originalEpoch!=session.epoch();
        if (replay && replaced) {
            ++revision;
            history={};
            nav->view.selected=0;
            edit.selected.reset();
            edit.seedInput=session.seed();
            controls.replaceInstance(heldKeys,session.paused());
            gui->invalidateSelection();
            previous=Clock::now();
        }
        if (replay && transportCommand) {
            controls.clearElapsed();
            previous=Clock::now();
        }
        if (beforeAdvanceEpoch!=session.epoch()) recordBuild();
        const auto& map=session.map();
        info.revision=revision;
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
            offerWindow=nav->drawOffer(finite || (replay && (replaced || transportCommand)),focused,controls,cpu);
            cpu.build+=micros(startOffer);
        }
        if (replay && !options.metrics.empty()) nav->replay->observe(info,nav->live);
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
    const auto& session=world();
    if (replay) nav->replay->finish(options,nav->live);
    report(samples);
    csv.close();
    std::cout<<"status=ok scene="<<(replay?"replay":navigation?"navigation":"map")<<" frames="<<frames<<" ticks="<<session.tick()<<" seed="<<session.map().seed()
             <<" epoch="<<session.epoch()<<" map_size="<<session.map().width()<<'x'<<session.map().height()
             <<" script="<<((nav?options.navWalk:options.mapWalk)?"walk":"none")<<" shortest_path="<<session.connectivity().shortestPathLength;
    if (nav) nav->printStatus();
    std::cout<<" timestamp_bits="<<(queries?queries->bits:-1)<<" valid_gpu_samples="<<(timer?timer->counts.completed:0)<<'\n';
}
}
