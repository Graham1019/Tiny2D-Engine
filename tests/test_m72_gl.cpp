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
            History::append(history.gpu,{f,frame,status,us});
        });
        if (queries->bits && frame>1) c.poll=micros(start);
        auto building=Clock::now();
        routed=gui.build(tools,n,1,d,controls.paused,history,timer.counts,queries->reason,true);
        c.build=micros(building);
        action=controls.poll(routed.keys,true,1./60,d,routed.captureKeyboard,routed.commands);
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
    pixels(window); imageOrientation(window); interaction(window,argc==2?argv[1]:"");
} catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;} }
