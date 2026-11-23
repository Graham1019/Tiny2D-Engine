#include "M5Test.h"
#include "ParticlePresentation.h"
#include "imgui_impl_glfw.h"
#include <cmath>
#include <iostream>

namespace {
using namespace lab;
Options args(std::initializer_list<const char*> values) {
    std::vector<const char*> argv{"lab"}; argv.insert(argv.end(),values.begin(),values.end());
    return parse(int(argv.size()),argv.data());
}
void arguments() {
    check(args({}).scene==LabScene::Neon && !args({"--scene","neon"}).display.lighting,"Neon CLI defaults");
    auto o=args({"--particle-steady","0","--particle-capacity","8192","--scene","particles","--particle-script","demo"});
    check(o.scene==LabScene::Particles && o.display.lighting && o.particleSteady==0 && o.particleDemo,
          "particle CLI config / exact zero");
    rejects([]{args({"--particle-rate","60001","--scene","particles"});});
    rejects([]{args({"--particle-capacity","0","--scene","particles"});});
    rejects([]{args({"--particle-steady","4097","--scene","particles"});});
    rejects([]{args({"--particle-script","demo"});});
    rejects([]{args({"--scene","particles","--shadows","on"});});
    rejects([]{args({"--scene","particles","--view","normal"});});
    ParticleSimulation sim;
    for (unsigned phase=0;phase<5;++phase) {
        sim.step(particleDemoTick(sim));
        check(sim.activeFields()==(phase==4 ? 4 : 1),"scripted field count");
        if (phase<4) check(sim.fields()[phase].enabled,"scripted field index");
        for (unsigned i=1;i<180;++i) sim.step(particleDemoTick(sim));
    }
}
void routedEvents(tiny2d::Window& window,bool finite) {
    Options o=args({"--scene","particles"});
    if (finite) o.frames=3;
    Inspector gui(window,finite);
    ParticleSimulation sim;
    ParticleEditor edit;
    edit.selected=2; // Vortex is unobstructed to the left of the Inspector.
    Controls controls;
    ToolState tools{true,false,false};
    auto* w=window.nativeHandle();
    ImGui_ImplGlfw_CursorEnterCallback(w,GLFW_TRUE);
    const auto logical=window.size();
    const auto camera=particleCamera(logical);
    const auto start=sim.fields()[2].position;
    const float x=logical.width*.5f+start.x*camera.zoom();
    const float y=logical.height*.5f+start.y*camera.zoom();
    auto frame=[&] {
        if (window.takeFocusLoss()) {controls.poll({},false,0,o.display); edit.dragging=false;}
        auto routed=gui.buildPanel(tools,true,[&](Commands& cmd){particlePanel(tools,edit,cmd,sim,o,controls.paused,{},"off");});
        auto action=controls.poll(routed.keys,true,1./60,o.display,routed.captureKeyboard,routed.commands,routed.freshFocus);
        edit.pointer(routed,sim,camera,logical,o.display.view);
        gui.discard();
        return action;
    };
    auto move=[&](float mx,float my){ ImGui_ImplGlfw_CursorPosCallback(w,mx,my); frame(); };
    auto button=[&](int action){ ImGui_ImplGlfw_MouseButtonCallback(w,0,action,0); frame(); };
    frame(); frame();
    move(x,y); button(GLFW_PRESS); move(x-40,y+15);
    if (finite) {
        ImGui_ImplGlfw_KeyCallback(w,GLFW_KEY_R,0,GLFW_PRESS,0);
        ImGui_ImplGlfw_KeyCallback(w,GLFW_KEY_F2,0,GLFW_PRESS,0);
        check(!frame().reset && tools.visible && !edit.dragging && !edit.pending.fields,"finite inspector rejects mouse/keys");
        return;
    }
    check(edit.dragging && edit.pending.fields && sim.fields()[2].position.x==start.x,"drag is pending until a tick");
    sim.step(edit.takeTick(sim,{}));
    check(std::abs(sim.fields()[2].position.x-(start.x-40/camera.zoom()))<.01f &&
          std::abs(sim.fields()[2].position.y-(start.y+15/camera.zoom()))<.01f,
          "logical drag maps exactly once through the camera");
    const auto focus=glfwSetWindowFocusCallback(w,nullptr); glfwSetWindowFocusCallback(w,focus);
    focus(w,GLFW_FALSE); focus(w,GLFW_TRUE); frame();
    move(x+10,y);
    check(!edit.dragging && !edit.pending.fields,"focus loss (and same-poll gain) cancels dragging");
    button(GLFW_RELEASE);
    move(logical.width-290.f,142); button(GLFW_PRESS);
    move(x-40,y+15); move(x,y);
    check(!edit.dragging && !edit.pending.fields,"held Inspector press cannot move world field");
    button(GLFW_RELEASE);
    move(x-40,y+15); button(GLFW_PRESS); move(x,y);
    check(edit.dragging && edit.pending.fields,"fresh scene press after release is accepted");
    button(GLFW_RELEASE);
    check(!edit.dragging,"release ends drag");
}
}
int main() {
    try {
        arguments();
        tiny2d::Window window(800,450,"Particle input regression");
        window.pollEvents();
        routedEvents(window,false);
        routedEvents(window,true);
        std::cout<<"Particle CLI, scripted fields, logical drag, tick-boundary edits, capture, focus and finite input passed\n";
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
