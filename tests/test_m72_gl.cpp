#include "M5Test.h"
#include "Inspector.h"
#include "Capture.h"
#include "OpenGL.h"
#include "imgui_impl_glfw.h"
#include <cmath>
#include <filesystem>
#include <iostream>

using namespace tiny2d;
using namespace lab;
void postCocoaResetKey(GLFWwindow*, bool down, bool repeat);
namespace lab {
struct PipelineProbe {
    static std::array<unsigned,4> hdrNames(const Pipeline& p) {
        const auto& g=*p.targets_;
        return {g.scene.colorName(),g.bright.colorName(),g.ping.colorName(),g.pong.colorName()};
    }
    static void checkpoint(Pipeline& p, std::function<void(const RenderTarget2D&)> f) {
        p.allocationCheckpoint_=std::move(f);
    }
};
}
namespace {
using Pixel=std::array<unsigned char,4>;
Pixel read(unsigned texture,int x,int y) {
    GLint saved=0; glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&saved);
    GLuint fbo=0;
    if (texture) {
        glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    } else { glBindFramebuffer(GL_READ_FRAMEBUFFER,0); glReadBuffer(GL_BACK); }
    Pixel p; glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER,saved); if (fbo) glDeleteFramebuffers(1,&fbo);
    return p;
}
void pixels(Window& window) {
    Pipeline pipe; pipe.resize({120,80});
    auto previews=pipe.previews(); Picture scene; Stats stats; Display d;
    scene.worldCamera.setViewportSize(120,80); scene.worldCamera.setPosition(60,40);
    scene.hudCamera=scene.worldCamera;
    scene.world={{{12,5},{18,16},0,{4,2,.2f,1}}};
    scene.hud={{{11,5},{18,16},0,{0,0,1,1}}};
    tiny2d::Scene fixture; auto id=fixture.create(); auto* obj=fixture.find(id);
    obj->transform.position={11,17}; obj->collider=AABB{{3,5},{43,35}};
    std::vector<DebugLine> lines;
    check(outlineCollider(fixture,id,lines) && lines.size()==4 && lines[0].from.x==14 &&
          lines[0].from.y==22 && lines[2].from.x==54 && lines[2].from.y==52,"real world AABB");
    scene.debugLines(lines,{1,0,0,1});
    d.bloom=false;
    for (bool srgb:{true,false}) {
        if (srgb) glEnable(GL_FRAMEBUFFER_SRGB); else glDisable(GL_FRAMEBUFFER_SRGB);
        pipe.draw(scene,d,stats,{15});
        check(stats.previewMask==15 && stats.passDraws[Preview]==4 && stats.passDraws[Debug]==1 &&
              stats.passDraws[Vertical]==1 && glIsEnabled(GL_FRAMEBUFFER_SRGB)==srgb,"demand routing and sRGB");
        auto sample=[&](unsigned v,int x,int y){return read(previews[v].texture,x,y);};
        auto hdr=sample(0,20,67);
        check(std::abs(int(hdr[0])-231)<=2 && hdr==sample(3,20,67) && sample(0,20,12)[0]<20,"Reinhard asymmetric marker");
        check(sample(1,20,67)[0]>80 && sample(2,20,67)[0]>60,"intermediates while bloom off");
        check(read(0,20,67)[2]>250 && read(0,36,57)[0]>240 && sample(3,36,57)[0]<20,
              "HUD and actual collider excluded from Final preview");
    }
    obj->transform.position={1,2}; obj->collider=Circle{{2,3},6}; lines.clear();
    check(outlineCollider(fixture,id,lines) && lines[0].from.x==9 && lines[0].from.y==5,"moving real circle");
    fixture.destroy(id); check(!outlineCollider(fixture,id,lines),"stale collider");
    scene.world.clear(); pipe.draw(scene,d,stats,{15});
    check(read(previews[1].texture,20,67)[0]==0 && read(previews[2].texture,20,67)[0]==0,"current intermediate frame");
    auto generation=pipe.generation(),bytes=pipe.storage();
    auto oldHDR=PipelineProbe::hdrNames(pipe);
    std::vector<GLuint> names;
    PipelineProbe::checkpoint(pipe,[&](const RenderTarget2D& target) {
        names.push_back(target.colorName());
        // The next production GLObject constructor must report this error.
        if (names.size()==2) glEnable(GL_NONE);
    });
    glViewport(3,5,19,23); GLint previous=0; glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&previous);
    rejects([&]{pipe.resize({137,81});});
    GLint vp[4],draw=0; glGetIntegerv(GL_VIEWPORT,vp); glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
    check(vp[0]==3 && vp[1]==5 && vp[2]==19 && vp[3]==23 && draw==previous,"partial allocation binding restoration");
    check(names.size()==2 && !glIsTexture(names[0]) && !glIsTexture(names[1]) && pipe.size().width==120 &&
          pipe.storage()==bytes && PipelineProbe::hdrNames(pipe)==oldHDR && pipe.generation()==generation && glIsTexture(previews[3].texture),"rollback preserves old group");
    rejects([&]{pipe.previews();});
    PipelineProbe::checkpoint(pipe,{});
    pipe.resize({120,80}); check(pipe.previews()[3].texture==previews[3].texture,"old preview safe after retry");
    pipe.resize({137,81}); pipe.previews(); pipe.draw(scene,d,stats,{15});
    check(stats.complete && !glIsTexture(previews[3].texture),"replacement and next frame");
    glDisable(GL_FRAMEBUFFER_SRGB); window.makeContextCurrent();
}
// Follow the actual Image command to its screen rectangle, then read an
// asymmetric marker from the default framebuffer after the official backend draws.
void imageOrientation(Window& window) {
    Inspector gui(window,true);
    Pipeline pipe;
    const Size pixels=window.framebufferSize();
    pipe.resize(pixels);
    Neon n(71); Display d; d.hud=false; d.bloom=false;
    ToolState tools; tools.previews=true; History history;
    Picture picture; picture.worldCamera.setViewportSize(120,80); picture.worldCamera.setPosition(60,40);
    picture.world={{{12,5},{18,16},0,{4,2,.2f,1}}};
    detail::GLObject unpack(detail::ObjectKind::Buffer);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,unpack.id());
    glPixelStorei(GL_UNPACK_SKIP_ROWS,2); glPixelStorei(GL_UNPACK_SKIP_PIXELS,3);
    for (int i=0;i<3;++i) {
        bool srgb=i!=1;
        if (srgb) glEnable(GL_FRAMEBUFFER_SRGB); else glDisable(GL_FRAMEBUFFER_SRGB);
        ImGui_ImplGlfw_KeyCallback(window.nativeHandle(),GLFW_KEY_F2,0,GLFW_PRESS,0);
        ImGui_ImplGlfw_CharCallback(window.nativeHandle(),'R');
        ImGui_ImplGlfw_ScrollCallback(window.nativeHandle(),0,5);
        auto input=gui.build(tools,n,1,d,false,history,{},"off",true);
        check(tools.visible && tools.previews && !input.keys.any() && !input.commands.edges.any() && !input.click,
              "finite input isolation");
        unsigned demand=gui.finish(tools,pipe,i+1,n.tick);
        Stats stats;
        pipe.draw(picture,d,stats,{demand,nullptr,[&](Stats& s){gui.submit(s);}});
        GLint rows,skip,pbo;
        glGetIntegerv(GL_UNPACK_SKIP_ROWS,&rows); glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skip);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&pbo);
        check(rows==2 && skip==3 && pbo==unpack.id() && glIsEnabled(GL_FRAMEBUFFER_SRGB)==srgb,"GUI unpack/sRGB state");
        if (!i) continue;
        const auto scene=pipe.previews()[0];
        auto* data=ImGui::GetDrawData(); bool found=false;
        for (auto* list:data->CmdLists) for (auto& cmd:list->CmdBuffer) {
            if (cmd.GetTexID()!=scene.texture) continue;
            auto pos=[&](int j){return list->VtxBuffer[cmd.VtxOffset+list->IdxBuffer[cmd.IdxOffset+j]].pos;};
            auto tl=pos(0),br=pos(2);
            int x=int((tl.x+.17f*(br.x-tl.x))*data->FramebufferScale.x);
            int y=int((tl.y+.16f*(br.y-tl.y))*data->FramebufferScale.y);
            auto color=read(0,x,pixels.height-1-y);
            check(std::abs(int(color[0])-231)<4 && std::abs(int(color[1])-213)<4,"displayed UV and single encoding");
            found=true;
        }
        check(found && stats.previewMask==15,"visible four image commands");
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);glDisable(GL_FRAMEBUFFER_SRGB);
}
void focusBatches(Window& window) {
    Inspector gui(window,false);
    Neon neon(71); Controls controls; Display display; ToolState tools;
    auto* w=window.nativeHandle();
    auto tick=[&](bool focused=true) {
        if (window.takeFocusLoss()) controls.poll({},false,0,display);
        auto input=gui.build(tools,neon,1,display,controls.paused,{},{},"off",focused);
        auto action=controls.poll(input.keys,focused,1./60,display,input.captureKeyboard,input.commands,input.freshFocus);
        gui.discard();
        return action;
    };
    auto key=[&](int code,int action){ImGui_ImplGlfw_KeyCallback(w,code,0,action,0);};
    auto tap=[&](int code){key(code,GLFW_PRESS);key(code,GLFW_RELEASE);};
    // GLFW has no callback getter. Borrow and immediately restore the installed
    // chain so injected focus events enter through the same boundary as OS events.
    const auto focus=glfwSetWindowFocusCallback(w,nullptr);
    glfwSetWindowFocusCallback(w,focus);
    auto refocus=[&] { focus(w,GLFW_FALSE); focus(w,GLFW_TRUE); };

    tick(); tick();
    // One GLFW poll can enqueue all these events before the next ImGui NewFrame.
    tap(GLFW_KEY_W); tap(GLFW_KEY_R); tap(GLFW_KEY_P); tap(GLFW_KEY_B); tap(GLFW_KEY_F2);
    refocus();
    int resets=0, pausedFrames=0, bloomOffFrames=0, hiddenFrames=0, movement=0;
    for (int i=0;i<16;++i) {
        auto a=tick();
        resets+=a.reset; pausedFrames+=controls.paused; bloomOffFrames+=!display.bloom;
        hiddenFrames+=!tools.visible; movement+=std::abs(a.motion.y);
    }
    std::cout<<"pre-loss batch after restore: resets="<<resets<<" paused_frames="<<pausedFrames
             <<" bloom_off_frames="<<bloomOffFrames<<" hidden_frames="<<hiddenFrames
             <<" movement="<<movement<<'\n';
    check(!resets && !pausedFrames && !bloomOffFrames && !hiddenFrames && !movement,
          "commands from the previous focus cycle were replayed");

    key(GLFW_KEY_W,GLFW_PRESS); check(tick().motion.y==-1,"W moves before focus loss");
    tap(GLFW_KEY_P); refocus();
    // A new R press+release belongs to the new focus cycle, even in the same poll.
    tap(GLFW_KEY_R);
    resets=0;
    for (int i=0;i<6;++i) {
        auto a=tick(); resets+=a.reset;
        check(!a.motion.y && !controls.paused,"held W and queued P suppressed");
    }
    check(resets==1,"fresh R immediately after gain survives the recovery boundary");
    key(GLFW_KEY_W,GLFW_RELEASE); tick();
    key(GLFW_KEY_W,GLFW_PRESS); check(tick().motion.y==-1,"release then re-press W after restore");
    key(GLFW_KEY_W,GLFW_RELEASE); tick();

    for (int code:{GLFW_KEY_W,GLFW_KEY_R,GLFW_KEY_F2}) key(code,GLFW_PRESS);
    tick();
    const bool wasVisible=tools.visible;
    focus(w,GLFW_FALSE);
    // GLFW documents synthetic releases after the loss callback. A held key's
    // GLFW_REPEAT is not a new key press in the official backend.
    for (int code:{GLFW_KEY_W,GLFW_KEY_R,GLFW_KEY_F2}) key(code,GLFW_RELEASE);
    tick(false);
    tap(GLFW_KEY_B); // Inactive input may also be queued before a later poll restores focus.
    focus(w,GLFW_TRUE);
    for (int i=0;i<4;++i) {
        for (int code:{GLFW_KEY_W,GLFW_KEY_R,GLFW_KEY_F2}) key(code,GLFW_REPEAT);
        auto a=tick();
        check(!a.reset && !a.motion.y && tools.visible==wasVisible && display.bloom,
              "held repeats and unfocused transitions cannot arm scene/tool keys");
    }
    for (int code:{GLFW_KEY_W,GLFW_KEY_R,GLFW_KEY_F2}) key(code,GLFW_RELEASE);
    tick();
    key(GLFW_KEY_R,GLFW_PRESS); check(tick().reset && !tick().reset,"fresh R fires once");
    key(GLFW_KEY_R,GLFW_RELEASE); tick();
    key(GLFW_KEY_F2,GLFW_PRESS); tick(); check(tools.visible!=wasVisible,"fresh F2 edge rearmed");
    key(GLFW_KEY_F2,GLFW_RELEASE); tick();
    std::cout<<"fresh same-poll R="<<resets<<", held W/R/F2 repeats filtered, release/re-press accepted\n";

    glfwFocusWindow(w); glfwPollEvents(); tick();
    postCocoaResetKey(w,true,false);
    check(glfwGetKey(w,GLFW_KEY_R)==GLFW_PRESS && tick().reset,"native R arms before focus loss");
    focus(w,GLFW_FALSE);
    postCocoaResetKey(w,false,false); // Put GLFW in the same state as its synthetic focus-loss release.
    focus(w,GLFW_TRUE);
    refocus(); // The key can stay physically held across another focus cycle.
    postCocoaResetKey(w,true,true);
    const bool replayed=tick().reset;
    std::cout<<"Cocoa auto-repeat with cleared GLFW key cache: reset="<<replayed<<'\n';
    check(!replayed,"first native repeat misclassified as a fresh GLFW_PRESS");
    postCocoaResetKey(w,false,false); tick();
    postCocoaResetKey(w,true,false); check(tick().reset,"native release/re-press rearmed R");
    focus(w,GLFW_FALSE);
    postCocoaResetKey(w,false,false);
    focus(w,GLFW_TRUE);
    postCocoaResetKey(w,true,false);
    check(tick().reset && !tick().reset,"physical release while away and fresh R on return");
    postCocoaResetKey(w,false,false); tick();

    struct RestoreCursor {
        GLFWwindow* window;
        double x,y;
        ~RestoreCursor() { glfwSetCursorPos(window,x,y); glfwPollEvents(); }
    } restore{w,0,0};
    glfwGetCursorPos(w,&restore.x,&restore.y);
    glfwSetCursorPos(w,522,112); glfwPollEvents();
    ImGui_ImplGlfw_CursorEnterCallback(w,GLFW_TRUE);
    ImGui_ImplGlfw_CursorPosCallback(w,522,112); tick();
    auto button=[&](int action){ImGui_ImplGlfw_MouseButtonCallback(w,0,action,0);};
    button(GLFW_PRESS); tick();
    check(ImGui::IsAnyItemActive() && display.bloom,"Bloom checkbox pressed before focus loss");
    focus(w,GLFW_FALSE); button(GLFW_RELEASE); focus(w,GLFW_TRUE); tick();
    std::cout<<"held Bloom checkbox on refocus: bloom="<<display.bloom<<'\n';
    check(display.bloom,"focus-loss release must cancel pressed widget");
    const auto cursor=ImGui::GetIO().MousePos;
    check(std::abs(cursor.x-522)<2 && std::abs(cursor.y-112)<2,"stationary cursor restored");
    button(GLFW_PRESS); tick(); button(GLFW_RELEASE); tick();
    check(!display.bloom,"fresh stationary click after refocus toggles Bloom");
}
void interaction(Window& window,const std::filesystem::path& evidence) {
    auto* w=window.nativeHandle(); window.pollEvents();
    Pipeline pipe; pipe.resize(window.framebufferSize()); Inspector gui(window,false); ToolState tools;
    auto queries=openTimestampQueries(Timer::capacity); Timer timer(*queries);
    std::cout<<"real timestamp: "<<queries->reason<<'\n';
    Neon n(71); Controls controls; Display d; History history; Stats last;
    Presented shown; std::uint64_t frame=0; RoutedInput routed; Action action;
    auto step=[&](const char* image=nullptr) {
        ++frame;
        const auto start=Clock::now();
        CpuSample c;
        if (timer.hasOutstanding()) timer.poll([&](FrameInfo f,QueryStatus status,const Timer::Durations& us) {
            check(status==QueryStatus::Complete && us[0],"real available timestamp");
            history.recordGpu(1,{f,frame,status,us});
        });
        if (queries->bits && frame>1) c.poll=micros(start);
        auto building=Clock::now();
        routed=gui.build(tools,n,1,d,controls.paused,history,timer.counts,queries->reason,true);
        c.build=micros(building);
        action=controls.poll(routed.keys,true,1./60,d,routed.captureKeyboard,routed.commands,routed.freshFocus);
        Content rect{0,0,window.size()};
        if (tools.visible && routed.click) tools.selected=shown.pick(*routed.click,rect,d.view,pipe.generation(),n,1);
        auto updating=Clock::now();
        for(unsigned i=0;i<action.steps;++i)n.update(action.motion);
        c.update=micros(updating);
        auto preparing=Clock::now();
        Picture p; p.prepare(n,d,controls.paused,rect.size,window.framebufferSize(),last);
        auto list=objects(n,1); if (tools.visible) p.bounds(list,tools.selected,tools.bounds,tools.outline);
        c.prepare=micros(preparing);
        building=Clock::now();
        auto previews=gui.finish(tools,pipe,frame,n.tick);
        c.build+=micros(building);
        FrameInfo f{frame,n.tick,1,pipe.generation(),window.framebufferSize(),rect.size,d,tools.visible,true,false,previews};
        DrawTools draw; draw.previews=previews;
        if (tools.visible) draw.gui=[&](Stats& s){gui.submit(s);};
        if (timer.begin(f)==QueryStatus::Pending) { draw.timer=&timer; timer.mark(0,false); }
        pipe.draw(p,d,last,draw);
        if (draw.timer) {timer.mark(0,true);f.mask=last.mask;timer.finish(f,true);}
        f.mask=last.mask; c.info=f; c.stats=last;c.total=micros(start);c.hdrBytes=pipe.storage();c.previewBytes=pipe.previewBytes();
        History::append(history.cpu,c);
        if (image && !evidence.empty()) capture((evidence/image).string(),window.framebufferSize());
        window.swapBuffers();
        shown={list,p.worldCamera,rect,d.view,pipe.generation(),frame,n.tick};
    };
    ImGui_ImplGlfw_CursorEnterCallback(w,GLFW_TRUE);
    auto move=[&](double x,double y){ImGui_ImplGlfw_CursorPosCallback(w,x,y);step();};
    auto button=[&](bool down){ImGui_ImplGlfw_MouseButtonCallback(w,0,down?GLFW_PRESS:GLFW_RELEASE,0);step();};
    auto click=[&](double x,double y){move(x,y);button(true);button(false);step();};
    auto key=[&](int k,bool down){ImGui_ImplGlfw_KeyCallback(w,k,0,down?GLFW_PRESS:GLFW_RELEASE,0);step();};
    step(); step();
    check(last.guiVertices && last.passDraws[Gui] && last.vertexBytes==last.guiVertices*sizeof(ImDrawVert),"real backend draw/upload");
    click(522,112); check(!d.bloom,"Bloom checkbox via official backend");
    click(530,181); check(controls.paused,"Pause button");
    auto frozen=n.tick; click(590,181);check(n.tick==frozen+1 && controls.paused,"one GUI step");
    click(532,181);check(!controls.paused,"Resume button");
    move(520,111);button(true);move(338,237);button(false);
    check(!routed.click && !tools.selected,"UI press dragged over player");
    click(338,237);check(tools.selected && tools.selected->kind==ObjectKind::Player,"scene click from shown frame");
    click(605,90); step("selection.png");
    click(540,190); check(tools.selected && tools.selected->kind==ObjectKind::Relay,"captured list selection");

    click(536,132); ImGui_ImplGlfw_CharCallback(w,'R');step();
    auto before=n.tick;key(GLFW_KEY_R,true);
    check(!action.reset && routed.captureKeyboard && n.tick==before+1,"actual text focus filters R while running");
    key(GLFW_KEY_ENTER,true);key(GLFW_KEY_ENTER,false);
    check(!action.reset,"Return releases text focus without replaying held R");
    click(536,132);
    key(GLFW_KEY_W,true);key(GLFW_KEY_SPACE,true);key(GLFW_KEY_P,true);
    check(!action.motion.y && !action.motion.fire && !controls.paused,"held text shortcuts");
    key(GLFW_KEY_F2,true);check(!tools.visible && !action.reset,"F2 during capture");step();
    check(!tools.visible && !action.motion.y && !action.motion.fire && !last.passDraws[Gui],"held keys gated after close");
    key(GLFW_KEY_F2,false);key(GLFW_KEY_F2,true);check(tools.visible,"reopened panel");key(GLFW_KEY_F2,false);
    for (int k:{GLFW_KEY_R,GLFW_KEY_W,GLFW_KEY_SPACE,GLFW_KEY_P}) key(k,false);
    click(670,90);step("timing.png");
    key(GLFW_KEY_F2,true); check(!tools.visible,"global F2 edge");
    gui.suspend(); controls.poll({},false,0,d); step();
    check(!tools.visible && !action.steps,"focus restoration gates held F2 and clock");
    key(GLFW_KEY_F2,false);key(GLFW_KEY_F2,true);key(GLFW_KEY_F2,false);
    check(tools.visible,"released F2 rearmed");

    if (queries->bits) check(timer.counts.completed>0,"hardware completed timestamp pairs");
    else check(!queries->reason.empty() && !timer.counts.requested,"explicit hardware downgrade");
    gui.build(tools,n,1,d,false,history,timer.counts,queries->reason,true);
    tools.previews=true;gui.finish(tools,pipe,frame,n.tick);gui.discard();
    pipe.resize({0,0}); rejects([&]{gui.submit(last);}); pipe.resize(window.framebufferSize());
    step(); step();
    check(last.complete && last.previewMask==15,"discarded data and fresh GUI frame");
    Window other(32,32,"foreign");
    rejects([&]{gui.build(tools,n,1,d,false,history,timer.counts,queries->reason,true);});
    window.makeContextCurrent();
}
}
int main(int argc,char** argv) { try {
    Window window(800,450,"M7.2 Inspector integration");
    pixels(window); imageOrientation(window); focusBatches(window); interaction(window,argc==2?argv[1]:"");
} catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;} }
