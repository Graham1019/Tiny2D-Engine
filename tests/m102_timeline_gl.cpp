#include "M5Test.h"
#include "NavigationApp.h"
#include "OpenGL.h"
#include "imgui_impl_glfw.h"
#include "tiny2d_game/SnapshotCodec.h"
#include <unistd.h>

using namespace lab;
using namespace tiny2d;
using namespace tiny2d_game;
namespace fs=std::filesystem;

namespace {
struct Workspace {
    fs::path directory;
    Workspace() {
        auto name=(fs::temp_directory_path()/"tiny2d-timeline-gl-XXXXXX").string();
        if (!mkdtemp(name.data())) throw std::runtime_error("create test directory");
        directory=name;
    }
    ~Workspace() { std::error_code ignore; fs::remove_all(directory,ignore); }
};

void run(Window& window,const fs::path& path) {
    NavigationSession source(71,{},0);
    ReplayRecorder recorder(source);
    for (int i=0;i<600;++i) recorder.apply(source,ReplayCommand::advance(mapWalkInput(i)));
    const auto pending=source.captureSnapshot();
    const auto offer=*source.upgrades().offer();
    recorder.apply(source,ReplayCommand::choose({offer.epoch,offer.serial,UpgradeId::Vitality}));
    const auto chosen=source.captureSnapshot();
    for (int i=0;i<29;++i) recorder.apply(source,ReplayCommand::advance({1,0}));
    const auto original=encodeReplay(recorder.data());
    writeNewFile(path,original);

    Options options;
    options.scene=LabScene::Replay;
    options.replayPath=path.string();
    NavigationApp app(options,{});
    auto& transport=*app.replay;
    auto info=[&] {
        FrameInfo f;
        transport.observe(f,app.live);
        return f;
    };

    // Drive the production Timeline slider with real ImGui GLFW input. No seek
    // may publish a candidate until the drag releases, even across many frames.
    Inspector gui(window,false);
    ToolState tools{true,false,false};
    Pipeline pipeline;
    pipeline.resize(window.framebufferSize());
    Picture picture;
    float sliderX=0, sliderY=0, sliderWidth=0;
    auto frame=[&]() {
        ReplayAction command=ReplayAction::None;
        gui.buildPanel(tools,true,[&](Commands&) {
            ImGui::SetNextWindowPos({10,10},ImGuiCond_Always);
            ImGui::SetNextWindowSize({540,390},ImGuiCond_Always);
            ImGui::Begin("Timeline controls",nullptr,ImGuiWindowFlags_NoTitleBar);
            auto pos=ImGui::GetCursorScreenPos();
            sliderX=pos.x;
            sliderWidth=ImGui::GetContentRegionAvail().x;
            // Seven single-line rows precede the slider in this root view.
            sliderY=pos.y+7*ImGui::GetTextLineHeightWithSpacing()+ImGui::GetFrameHeight()*.5f;
            command=transport.timelinePanel(app.live);
            ImGui::End();
        });
        if (command==ReplayAction::None) transport.advancePlayback();
        else transport.perform(command,app.live);
        FrameInfo f;
        gui.finish(tools,pipeline,f,picture);
        Stats stats;
        DrawTools draw;
        draw.gui=[&](Stats& s){gui.submit(s);};
        pipeline.draw(picture,f.display,stats,draw);
        check(stats.complete && stats.guiVertices,"real Timeline UI submitted");
        return command;
    };
    auto* w=window.nativeHandle();
    ImGui_ImplGlfw_CursorEnterCallback(w,GLFW_TRUE);
    frame(); frame();
    ImGui_ImplGlfw_CursorPosCallback(w,sliderX+.2f*sliderWidth,sliderY);
    frame();
    const auto before=info();
    ImGui_ImplGlfw_MouseButtonCallback(w,0,GLFW_PRESS,0);
    check(frame()==ReplayAction::Pause,"slider press pauses transport immediately");
    ImGui_ImplGlfw_CursorPosCallback(w,sliderX+.6f*sliderWidth,sliderY);
    frame(); frame();
    check(info().eventSequence==before.eventSequence && info().simulationHash==before.simulationHash,
          "drag preview did not seek or advance");
    ImGui_ImplGlfw_MouseButtonCallback(w,0,GLFW_RELEASE,0);
    check(frame()==ReplayAction::Seek,"slider release commits exactly one seek");
    const auto after=info();
    check(after.eventSequence>330 && after.eventSequence<420 && transport.revision()>1,
          "displayed world moved to released event cursor");
    frame();
    check(info().simulationHash==after.simulationHash,"view is paused after release");

    transport.seek(601);
    check(firstDifference(chosen,app.displayed().captureSnapshot()).empty(),"seek shows parent Vitality");
    transport.perform(ReplayAction::Previous,app.live);
    check(firstDifference(pending,app.displayed().captureSnapshot()).empty(),"Prev returns pre-choice offer at same tick");
    const auto stale=transport.revision();
    transport.fork(102);
    const auto fleet=ReplayCommand::choose({offer.epoch,offer.serial,UpgradeId::Fleet});
    check(!transport.dispatchAtRevision(app.live,fleet,stale),"pre-fork pending choice rejected with identical logical identity");
    check(transport.dispatchAtRevision(app.live,fleet,transport.revision()),"fresh choice accepted in editing child");
    check(app.displayed().upgrades().lastChoice()->id==UpgradeId::Fleet && app.displayed().world().tick()==600,
          "fork chose alternate upgrade at same tick");
    check(app.dispatch(ReplayCommand::advance({0,1})),"child consumes real movement");
    check(app.dispatch(ReplayCommand::reset()),"child can reset");
    check(app.dispatch(ReplayCommand::regenerate(88,{16,12})),"child can regenerate");
    const auto childTip=app.displayed().captureSnapshot();
    const auto ticket=transport.revision();
    transport.perform(ReplayAction::Parent,app.live);
    check(transport.revision()>ticket && firstDifference(pending,app.displayed().captureSnapshot()).empty(),
          "switch to original parent retains its cursor and full state");
    transport.perform(ReplayAction::Next,app.live);
    check(firstDifference(chosen,app.displayed().captureSnapshot()).empty(),"parent future survives child edits");
    transport.perform(ReplayAction::Child,app.live);
    check(firstDifference(childTip,app.displayed().captureSnapshot()).empty(),"inspect independent child tip");
    transport.seek(1);
    check(app.displayed().world().tick()==600 && !app.dispatch(ReplayCommand::reset()),"past child is read-only");
    transport.perform(ReplayAction::ResumeChild,app.live);
    check(!transport.editingBranch(),"cannot overwrite child's recorded future");
    transport.perform(ReplayAction::End,app.live);
    transport.perform(ReplayAction::ResumeChild,app.live);
    check(transport.editingBranch() && app.dispatch(ReplayCommand::setPaused(true)),"explicit child tip resumes");
    const auto current=info();
    rejects([&]{transport.seek(1000000);});
    check(transport.editingBranch() && current.simulationHash==info().simulationHash,"failed seek preserves editing state");
    check(original==readBoundedFile(path,maxReplayBytes),"parent disk recording unchanged");
    std::cout<<"Drag paused at cursor "<<before.eventSequence<<" and committed once at "<<after.eventSequence
             <<"; same-tick fork, stale input, immutable parent, branch switching and reset/regenerate passed\n";
}
}
int main() {
    try {
        Workspace work;
        Window window(600,420,"M10.2 Timeline");
        run(window,work.directory/"parent.replay");
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
