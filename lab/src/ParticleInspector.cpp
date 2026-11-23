#include "ParticlePresentation.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace lab {
namespace {
using namespace ImGui;
constexpr auto clamp=ImGuiSliderFlags_AlwaysClamp|ImGuiSliderFlags_NoInput;
using U64=unsigned long long;
void playback(const bool paused,Commands& commands) {
    if (Button(paused?"Resume (P)":"Pause (P)")) commands.edges.set(Pause);
    SameLine(); BeginDisabled(!paused);
    if (Button("Step (.)")) commands.edges.set(Step);
    EndDisabled(); SameLine();
    if (Button("Reset (R)")) commands.edges.set(Reset);
}
void toggle(const char* label,bool value,Key key,Commands& commands) {
    if (Checkbox(label,&value)) commands.edges.set(key);
}
bool uintSlider(const char* label,unsigned& value,int min,int max) {
    int temp=int(value);
    if (!SliderInt(label,&temp,min,max,"%d",clamp)) return false;
    value=unsigned(temp);
    return true;
}
bool positionFields(tiny2d::Vector2& p) {
    bool changed=SliderFloat("World X",&p.x,-ParticleSimulation::halfWidth,ParticleSimulation::halfWidth,"%.1f",clamp);
    changed|=SliderFloat("World Y",&p.y,-ParticleSimulation::halfHeight,ParticleSimulation::halfHeight,"%.1f",clamp);
    return changed;
}
void emitterTab(ParticleEditor& edit,const ParticleSimulation& sim,const Options& o) {
    auto e=edit.pending.emitter.value_or(sim.emitter());
    if (Button("Move emitter")) { edit.selected=-1; edit.dragging=false; }
    if (o.particleSteady) Text("Steady target: %u (overrides rate / burst)",*o.particleSteady);
    BeginDisabled(o.particleSteady.has_value());
    bool changed=Checkbox("Emitter enabled",&e.enabled);
    changed|=uintSlider("Rate / s",e.rate,0,ParticleSimulation::maxRate);
    EndDisabled();
    changed|=positionFields(e.position);
    changed|=uintSlider("Lifetime / ticks",e.lifetime,1,3600);
    changed|=SliderFloat("Initial speed",&e.speed,0,ParticleSimulation::maxSpeed,"%.1f",clamp);
    changed|=SliderFloat("Direction",&e.direction,-180,180,"%.0f deg",clamp);
    changed|=SliderFloat("Half spread",&e.spread,0,180,"%.0f deg",clamp);
    changed|=SliderFloat("Diameter",&e.size,.25f,40,"%.2f",clamp);
    changed|=SliderFloat("Emission",&e.emission,0,32,"%.2f",clamp);
    float color[]{e.color.r,e.color.g,e.color.b};
    if (ColorEdit3("Linear tint",color,ImGuiColorEditFlags_NoInputs)) {
        e.color={color[0],color[1],color[2]}; changed=true;
    }
    if (changed) edit.pending.emitter=e;
}
void fieldsTab(ParticleEditor& edit,const ParticleSimulation& sim) {
    auto fields=edit.pending.fields.value_or(sim.fields());
    char label[64]="Emitter";
    auto labelFor=[&](int slot) {
        std::snprintf(label,sizeof(label),"%d  %s%s",slot+1,fieldName(fields[slot].kind),fields[slot].enabled?"":" (off)");
    };
    if (edit.selected>=0) labelFor(edit.selected);
    if (BeginCombo("Selected",label)) {
        if (Selectable("Emitter",edit.selected==-1)) { edit.selected=-1; edit.dragging=false; }
        for (int i=0;i<int(fields.size());++i) {
            labelFor(i);
            if (Selectable(label,i==edit.selected)) { edit.selected=i; edit.dragging=false; }
        }
        EndCombo();
    }
    Checkbox("Drag in scene",&edit.dragEnabled);
    SameLine(); Checkbox("Guides",&edit.guides);
    if (edit.selected<0) { TextWrapped("Edit the emitter in Particles. WASD moves it on each tick."); return; }
    auto& f=fields[edit.selected];
    bool changed=Checkbox("Enabled",&f.enabled);
    int kind=int(f.kind);
    if (Combo("Field type",&kind,"Attractor\0Repulsor\0Vortex\0Uniform wind\0")) {
        f.kind=static_cast<FieldKind>(kind); changed=true;
    }
    if (f.kind==FieldKind::Wind) {
        TextWrapped("Wind acts everywhere. A zero direction produces calm air.");
        float v[]{f.direction.x,f.direction.y};
        if (SliderFloat2("Direction XY",v,-1,1,"%.2f",clamp)) {
            f.direction={v[0],v[1]}; changed=true;
        }
    } else {
        changed|=positionFields(f.position);
        changed|=SliderFloat("Radius",&f.radius,1,640,"%.1f",clamp);
    }
    changed|=SliderFloat("Strength",&f.strength,0,100000,"%.1f",clamp|ImGuiSliderFlags_Logarithmic);
    if (f.kind==FieldKind::Vortex) changed|=Checkbox("Clockwise",&f.clockwise);
    if (changed) edit.pending.fields=fields;
}
template<class Measure>
void timingRow(const char* label,const History& history,Measure get) {
    std::array<double,History::capacity> values;
    std::size_t n=0;
    for (const auto& sample:history.cpu) values[n++]=get(sample);
    std::sort(values.begin(),values.begin()+n);
    auto q=[&](double p) {return values[std::size_t(std::ceil(n*p))-1];};
    Text("%-9s %6.1f %6.1f %6.1f",label,q(.50),q(.95),q(.99));
}
void timingTab(const History& history,const ToolState& tools,const std::string& capability) {
    TextWrapped("GPU timestamp: %s",tools.timing ? capability.c_str() : "disabled");
    if (tools.timing && !history.gpu.empty() && history.gpu.back().us[0]) {
        const auto& g=history.gpu.back();
        Text("GPU frame #%llu: %.1f us",U64(g.info.id),*g.us[0]);
        if (auto us=g.us[1+ParticleEmission]) Text("GPU particles: %.1f us",*us);
        else TextUnformatted("GPU particles: N/A");
    } else TextUnformatted("GPU time: N/A");
    if (history.cpu.empty()) { TextDisabled("Waiting for the first completed frame."); return; }
    const auto& latest=history.cpu.back();
    Text("Valid GPU samples: %llu",U64(latest.validGpuSamples));
    SeparatorText("CPU submission (us)");
    TextDisabled("             p50    p95    p99");
    timingRow("Frame",history,[](const auto& c){return c.total;});
    timingRow("Update",history,[](const auto& c){return c.update;});
    timingRow("Forces",history,[](const auto& c){return c.forceUpdateUs;});
    timingRow("Snapshot",history,[](const auto& c){return c.snapshotPrepareUs;});
    timingRow("Upload",history,[](const auto& c){return c.stats.instanceUploadUs;});
    Text("Window: %zu frames",history.cpu.size());
    Text("Instances %u   upload %llu B",latest.stats.activeInstances,U64(latest.stats.instanceUploadBytes));
    Text("Draws: particles %u / total %llu",latest.stats.particleDrawCalls,U64(latest.stats.draws));
}
}
void particlePanel(ToolState& tools,ParticleEditor& edit,Commands& commands,const ParticleSimulation& sim,
                   const Options& o,bool paused,const History& history,const std::string& capability) {
    using namespace ImGui;
    const auto size=GetIO().DisplaySize;
    const float w=std::min(334.f,std::max(1.f,size.x-8)), top=std::min(42.f,size.y*.12f);
    SetNextWindowPos({std::max(0.f,size.x-w-6),top},ImGuiCond_FirstUseEver);
    SetNextWindowSize({w,std::max(48.f,size.y-top-48)},ImGuiCond_FirstUseEver);
    const auto flags=o.frames ? ImGuiWindowFlags_NoInputs : ImGuiWindowFlags_None;
    if (Begin("Particle Inspector",nullptr,flags)) {
        TextColored({.42f,.88f,.8f,1},"Pre-update tick %llu",U64(sim.tick()));
        SameLine(); Text("%u / %u active",sim.active(),sim.capacity());
        TextDisabled("Dropped %llu    Active fields %u / 8",U64(sim.dropped()),sim.activeFields());
        if (BeginTabBar("Particle panels")) {
            if (BeginTabItem("Controls")) {
                playback(paused,commands);
                toggle("Bloom (B)",o.display.bloom,Bloom,commands); SameLine();
                toggle("Lighting (L)",o.display.lighting,ToggleLighting,commands);
                toggle("HUD (F1)",o.display.hud,Hud,commands); SameLine();
                Checkbox("Force guides",&edit.guides);
                int view=int(o.display.view);
                const char* views[]={"1 Scene","2 Bright","3 Blur","4 Final","5 Light"};
                if (Combo("View",&view,views,o.display.lighting?5:4)) commands.view=View(view);
                TextWrapped("Views 6/7: N/A. This playground has no normal material or shadow caster.");
                Separator();
                TextWrapped("Select a field in Forces. Drag its center or move it with WASD. Space emits a burst.");
                Checkbox("GPU timing",&tools.timing);
                EndTabItem();
            }
            if (BeginTabItem("Particles")) {
                playback(paused,commands);
                BeginDisabled(o.particleDemo); emitterTab(edit,sim,o); EndDisabled();
                EndTabItem();
            }
            if (BeginTabItem("Forces")) {
                BeginDisabled(o.particleDemo); fieldsTab(edit,sim); EndDisabled();
                EndTabItem();
            }
            if (BeginTabItem("Timing")) { timingTab(history,tools,capability); EndTabItem(); }
            if (BeginTabItem("Buffers")) {
                Checkbox("Live buffer previews",&tools.previews);
                TextWrapped(o.display.lighting ? "Particles accumulate in linear RGBA16F Emission before SceneCompose." :
                            "Lighting off: particles accumulate directly in linear RGBA16F Scene.");
                if (!history.cpu.empty()) {
                    auto& c=history.cpu.back();
                    Text("Framebuffer %d x %d",c.info.pixels.width,c.info.pixels.height);
                    Text("HDR attachments %llu bytes",U64(c.hdrBytes));
                }
                TextWrapped("Bright isolates highlights; Blur shows their filtered radiance.");
                EndTabItem();
            }
            EndTabBar();
        }
        if (o.particleDemo) TextDisabled("Demo script drives simulation settings.");
        if (paused && (edit.pending.emitter || edit.pending.fields || edit.pending.burst))
            TextWrapped("Edited settings commit when a tick advances.");
        if (!edit.notice.empty()) TextWrapped("%s",edit.notice.c_str());
    }
    End();
}
}
