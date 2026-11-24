#include "M5Test.h"
#include "NavigationApp.h"
#include "OpenGL.h"
#include "imgui_impl_glfw.h"
#include "tiny2d_game/SnapshotCodec.h"
#include <unistd.h>
#include <filesystem>
#include <vector>

using namespace lab;
using namespace tiny2d;
using namespace tiny2d_game;

namespace lab {
struct PipelineProbe {
    static GLuint scene(const Pipeline& p) { return p.targets_->scene.colorName(); }
};
}

namespace {
namespace fs=std::filesystem;
struct TestDirectory {
    fs::path path;
    TestDirectory() {
        auto name=(fs::temp_directory_path()/"m101-gl-XXXXXX").string();
        if (!mkdtemp(name.data())) throw std::runtime_error("create temporary GL fixture");
        path=name;
    }
    ~TestDirectory() { std::error_code ec; fs::remove_all(path,ec); }
};

void transport(Window& window,const fs::path& directory) {
    Options options;
    options.scene=LabScene::Replay;
    options.navEnemies=0;
    NavigationApp app(options,{});
    auto& replay=*app.replay;
    for (unsigned i=0;i<600;++i) app.live.step();
    const auto base=app.live.captureSnapshot();
    const auto offer=*app.live.upgrades().offer();
    replay.perform(ReplayAction::StartRecording,app.live);
    app.dispatch(ReplayCommand::setPaused(true));
    app.dispatch(ReplayCommand::choose({offer.epoch,offer.serial,offer.choices[0]}));
    app.dispatch(ReplayCommand::manualStep({1,0}));
    replay.perform(ReplayAction::StopRecording,app.live);
    const auto recorded=app.live.captureSnapshot();
    replay.perform(ReplayAction::PlayFromBase,app.live);
    replay.perform(ReplayAction::Pause,app.live);
    check(firstDifference(base,app.displayed().captureSnapshot()).empty(),"restart restored the exact pending offer");
    check(!app.dispatch(ReplayCommand::setPaused(true)) && !app.dispatch(ReplayCommand::reset()) &&
          !app.dispatch(ReplayCommand::choose({offer.epoch,offer.serial,offer.choices[0]})),
          "viewer rejects authoritative live controls");
    Inspector gui(window,false);
    ToolState tools{true,false,false};
    Pipeline pipe;
    pipe.resize(window.framebufferSize());
    Picture pic;
    Display display;
    Controls controls;
    MapEditor edit{71};
    History history;
    auto frame=[&] {
        auto inputs=gui.buildPanel(tools,true,[&](Commands& cmd) {
            navigationPanel(tools,edit,app.view,cmd,app.displayed(),false,history,nullptr,
                            [&]{replay.panel(app.live);},replay.playback());
        },false,true);
        auto action=controls.poll(inputs.keys,true,0.25,display);
        CpuSample sample;
        app.advance(action,options,controls,sample);
        const bool overlay=app.drawOffer(false,true,controls,sample);
        FrameInfo info;
        gui.finish(tools,pipe,info,pic,{},overlay);
        Stats stats;
        DrawTools draw;
        if (tools.visible || overlay) draw.gui=[&](Stats& s){gui.submit(s);};
        pipe.draw(pic,display,stats,draw);
        check(stats.complete,"replay overlay draws to owner framebuffer");
    };
    frame(); frame();
    auto* w=window.nativeHandle();
    ImGui_ImplGlfw_KeyCallback(w,GLFW_KEY_D,0,GLFW_PRESS,0);
    ImGui_ImplGlfw_KeyCallback(w,GLFW_KEY_F2,0,GLFW_PRESS,0);
    frame();
    ImGui_ImplGlfw_KeyCallback(w,GLFW_KEY_F2,0,GLFW_RELEASE,0);
    frame();
    check(!tools.visible && firstDifference(base,app.displayed().captureSnapshot()).empty(),
          "held real movement and F2 leave paused playback unchanged");
    ImGui_ImplGlfw_KeyCallback(w,GLFW_KEY_F2,0,GLFW_PRESS,0);
    frame();
    ImGui_ImplGlfw_KeyCallback(w,GLFW_KEY_F2,0,GLFW_RELEASE,0);
    ImGui_ImplGlfw_KeyCallback(w,GLFW_KEY_D,0,GLFW_RELEASE,0);
    frame();
    check(tools.visible,"F2 restores transport controls");
    replay.perform(ReplayAction::Next,app.live);
    check(app.displayed().world().paused() && app.displayed().phase()==NavigationPhase::AwaitingUpgrade &&
          app.displayed().world().tick()==600,"one button advanced only the same-tick pause");
    frame();
    check(app.displayed().phase()==NavigationPhase::AwaitingUpgrade,"transport remains paused after single event");
    replay.perform(ReplayAction::Next,app.live);
    check(app.displayed().phase()==NavigationPhase::Playing && app.displayed().hp()==6 &&
          app.displayed().world().tick()==600,"one button applied Vitality at 600");
    replay.perform(ReplayAction::Resume,app.live);
    frame(); frame();
    check(firstDifference(recorded,app.displayed().captureSnapshot()).empty(),"finished playback cannot overshoot");
    replay.perform(ReplayAction::Restart,app.live);
    check(firstDifference(base,app.displayed().captureSnapshot()).empty(),"restart resets verified cursor and world");
    writeNewFile(directory/"bad",{0,1,2});
    rejects([&]{replay.loadReplay(directory/"bad");});
    rejects([&]{replay.loadSnapshot(directory/"bad",app.live);});
    frame();
    check(firstDifference(base,app.displayed().captureSnapshot()).empty(),"rejected files preserve displayed playback");
    replay.perform(ReplayAction::ReturnLive,app.live);
    check(firstDifference(recorded,app.displayed().captureSnapshot()).empty(),"original live session survives viewing");

    options.recordPath=(directory/"launch.replay").string();
    NavigationApp launch(options,{});
    launch.dispatch(ReplayCommand::advance({1,0}));
    launch.replay->perform(ReplayAction::StopRecording,launch.live);
    launch.replay->perform(ReplayAction::StartRecording,launch.live);
    launch.dispatch(ReplayCommand::reset());
    launch.replay->finish(options,launch.live);
    const auto written=decodeReplay(readBoundedFile(options.recordPath,maxReplayBytes));
    check(written.events.size()==1 && written.events[0].command.type()==ReplayEventType::AdvanceTick &&
          written.base.world.tick==0,"CLI exports the recording from launch after UI opens a fresh tape");
}

void restoredPixels(Window& window,const fs::path& directory) {
    Options options;
    options.scene=LabScene::Replay;
    options.navEnemies=4;
    NavigationApp app(options,MapParameters{16,12});
    for (int i=0;i<9;++i) app.live.step(mapWalkInput(app.live.world().tick()));
    const auto snapshot=app.live.captureSnapshot();
    writeNewFile(directory/"middle",encodeSnapshot(snapshot));
    GLuint owned=0;
    {
        Pipeline pipe;
        auto draw=[&]() {
            const auto& world=app.displayed().world();
            const auto pixels=window.framebufferSize(),logical=window.size();
            pipe.resize(pixels);
            Picture picture;
            picture.worldCamera=mapCamera(world.map(),world.playerPosition(),logical);
            const auto tiles=prepareMapTiles(picture,world.map(),logical);
            prepareMapActors(picture,world.map(),world.playerPosition(),world.playerHalfSize(),tiles,false,{});
            CpuSample sample;
            sample.info.logical=logical;
            app.prepare(picture,tiles,options,sample);
            Stats stats;
            glEnable(GL_SCISSOR_TEST);
            glViewport(3,4,17,18);
            pipe.draw(picture,options.display,stats);
            GLint vp[4];
            glGetIntegerv(GL_VIEWPORT,vp);
            check(glIsEnabled(GL_SCISSOR_TEST) && vp[0]==3 && vp[3]==18,"restored GL state");
            glDisable(GL_SCISSOR_TEST);
            check(stats.submittedEnemies>0,"restored enemies submitted");
            std::vector<std::uint8_t> rgba(std::size_t(pixels.width)*pixels.height*4);
            glReadPixels(0,0,pixels.width,pixels.height,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
            detail::checkGL("replay pixels");
            return rgba;
        };
        for (int width:{256,384}) {
            glfwSetWindowSize(window.nativeHandle(),width,width*3/4);
            window.pollEvents();
            const auto initial=draw();
            const auto old=owned;
            owned=PipelineProbe::scene(pipe);
            check(!old || !glIsTexture(old),"resize retired prior owned texture");
            for (int i=0;i<12;++i) app.live.step({1,0});
            check(initial!=draw(),"motion changes framebuffer");
            app.replay->loadSnapshot(directory/"middle",app.live);
            check(initial==draw() && firstDifference(snapshot,app.displayed().captureSnapshot()).empty(),
                  "restored player and enemies reproduce the saved pixels");
            std::cout<<"restored_pixels logical="<<width<<" framebuffer="<<window.framebufferSize().width<<'\n';
        }
        {
            Window other(64,64,"Replay foreign context");
            rejects([&]{pipe.resize({32,32});});
        }
        window.makeContextCurrent();
        check(glIsTexture(owned),"foreign context preserved owner texture");
        draw();
    }
    check(!glIsTexture(owned),"replay owner released final texture");

    History history;
    FrameInfo old;
    old.epoch=snapshot.world.epoch;
    old.revision=3;
    GpuSample retired{old,5,QueryStatus::Complete,{}};
    history.recordGpu(old.epoch,retired,4);
    check(history.gpu.empty(),"same historical epoch from old instance cannot populate new history");
    retired.info.revision=4;
    history.recordGpu(old.epoch,retired,4);
    check(history.gpu.size()==1,"current instance accepts completed query");
    Controls controls;
    Keys held;
    held.set(Right);
    controls.replaceInstance(held,false);
    check(!controls.poll(held,true,0,options.display).motion.x,"restore gates previously held input");
    controls.poll({},true,0,options.display);
    check(controls.poll(held,true,0,options.display).motion.x==1,"fresh press accepted");
}
}

int main() {
    try {
        TestDirectory files;
        Window window(600,420,"M10.1 Replay GL");
        transport(window,files.path);
        restoredPixels(window,files.path);
        std::cout<<"Replay transport, F2, input gating, restore pixels, query revision and GL ownership passed\n";
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
