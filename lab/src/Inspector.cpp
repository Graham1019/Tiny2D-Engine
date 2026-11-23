#include "Inspector.h"
#include "OpenGL.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace lab {
bool cocoaKeyRepeats(GLFWwindow*,int scancode);
namespace {
using namespace ImGui;
using U64=unsigned long long;
constexpr ImGuiKey keys[]={ImGuiKey_A,ImGuiKey_D,ImGuiKey_W,ImGuiKey_S,ImGuiKey_Space,ImGuiKey_B,
    ImGuiKey_1,ImGuiKey_2,ImGuiKey_3,ImGuiKey_4,ImGuiKey_LeftBracket,ImGuiKey_RightBracket,
    ImGuiKey_P,ImGuiKey_Period,ImGuiKey_R,ImGuiKey_F1,ImGuiKey_Escape,ImGuiKey_L,ImGuiKey_5,
    ImGuiKey_N,ImGuiKey_6,ImGuiKey_O,ImGuiKey_7};
static_assert(std::size(keys)==KeyCount);
void cancelPendingInput(ImGuiIO& io) {
    // A button normally fires on release. Cancel a pressed widget before
    // clearing MouseDown, even if the stationary cursor will still hover it.
    if (io.MouseDown[0] && ImGui::IsAnyItemActive()) ImGui::SetWindowFocus(nullptr);
    io.ClearEventsQueue();
    io.ClearInputKeys();
    io.ClearInputMouse();
}
bool rawDown(ImGuiKey k) { return GetIO().KeysData[k-ImGuiKey_NamedKey_BEGIN].Down; }
void shadowEnable(const Display& display,Commands& commands) {
    bool enabled=display.shadows;
    if (Checkbox("Shadows (O)",&enabled)) commands.edges.set(ToggleShadows);
}
void controls(const Display& d,bool paused,Commands& cmd,ToolState& tools) {
    bool bloom=d.bloom,hud=d.hud;
    if (Checkbox("Bloom",&bloom)) cmd.edges.set(Bloom);
    SameLine(); if (Checkbox("HUD",&hud)) cmd.edges.set(Hud);
    int strength=d.strength;
    char gain[24]; std::snprintf(gain,sizeof(gain),"%.1f",strength/10.);
    SetNextItemWidth(-74);
    if (SliderInt("Strength",&strength,0,30,gain,ImGuiSliderFlags_AlwaysClamp|ImGuiSliderFlags_NoInput))
        cmd.strength=strength;
    SetNextItemWidth(-74);
    const char* views[]={"Scene","Bright","Blur","Final","Light","Normal","Shadow"};
    if (BeginCombo("View",views[int(d.view)])) {
        for (auto view:{View::Scene,View::Bright,View::Blur,View::Final,View::Light,View::Normal,View::Shadow}) {
            if ((view==View::Light && !d.lighting) || (view==View::Normal && !d.normalMapping) ||
                (view==View::Shadow && !d.shadows)) continue;
            if (Selectable(views[int(view)],view==d.view)) cmd.view=view;
        }
        EndCombo();
    }
    if (Button(paused?"Resume":"Pause")) cmd.edges.set(Pause);
    SameLine(); BeginDisabled(!paused);
    if (Button("Step 1 tick")) cmd.edges.set(Step);
    EndDisabled(); SameLine(); if (Button("Reset")) cmd.edges.set(Reset);
    Separator();
    Checkbox("PickBounds",&tools.bounds); SameLine(); Checkbox("Outline",&tools.outline);
    Checkbox("Buffers",&tools.previews); SameLine(); Checkbox("GPU timing",&tools.timing);
    TextDisabled("Threshold 1   Gaussian r8 / 3.2");
    if (d.lighting) shadowEnable(d,cmd);
}
void lightingControls(const Display& display,Commands& commands,ToolState& tools) {
    bool enabled=display.lighting;
    if (Checkbox("Enabled (L)",&enabled)) commands.edges.set(ToggleLighting);
    if (!enabled) { TextWrapped("Enable Lighting to inspect the Light buffer (5)."); return; }
    auto slider=[](const char* label,float value,float low,float high,std::optional<float>& change) {
        SetNextItemWidth(-105);
        if (SliderFloat(label,&value,low,high,"%.2f",ImGuiSliderFlags_AlwaysClamp|ImGuiSliderFlags_NoInput))
            change=value;
    };
    slider("Ambient",display.ambient,0,Display::maxAmbient,commands.ambient);
    slider("Light strength",display.lightStrength,0,Display::maxLightStrength,commands.lightStrength);
    slider("Radius scale",display.radiusMultiplier,Display::minRadiusMultiplier,
           Display::maxRadiusMultiplier,commands.radiusMultiplier);
    Checkbox("Light radii",&tools.lightRadii); SameLine();
    if (Button(display.view==View::Light?"Show Final (4)":"Show Light (5)"))
        commands.view=display.view==View::Light?View::Final:View::Light;
    bool normals=display.normalMapping;
    if (Checkbox("Normal Mapping (N)",&normals)) commands.edges.set(ToggleNormal);
    if (!normals) { TextDisabled("Normal buffer off"); return; }
    SameLine();
    if (Button(display.view==View::Normal?"Final (4)":"Show Normal (6)"))
        commands.view=display.view==View::Normal?View::Final:View::Normal;
    slider("Normal strength",display.normalMaterial.strength,0,NormalMaterial2D::maxStrength,commands.normalStrength);
    int material=static_cast<int>(display.normalMaterial.kind);
    SetNextItemWidth(-105);
    if (Combo("Material",&material,"Flat\0Beveled tile\0"))
        commands.normalMaterial=static_cast<NormalMaterialKind>(material);
}
void lightDetails(ToolState& tools,const LightList& lights,std::uint64_t tick) {
    Text("Tick %llu   Active %zu / %zu   Dropped %zu",U64(tick),std::size_t(lights.size()),
         std::size_t(LightList::capacity),std::size_t(lights.dropped()));
    const Light2D* selected=tools.selected?lights.find(*tools.selected):nullptr;
    // With no scene selection the Inspector follows the player without
    // changing the picked object or its post-process outline.
    if (!tools.selected) {
        for (const auto& light:lights)
            if (light.source.kind==ObjectKind::Player) { selected=&light; break; }
    }
    char label[64]="Select a source";
    if (selected) std::snprintf(label,sizeof(label),"%s %u",name(selected->source.kind),selected->source.slot);
    SetNextItemWidth(-64);
    if (BeginCombo("Source",label)) {
        for (const auto& light:lights) {
            std::snprintf(label,sizeof(label),"%s %u",name(light.source.kind),light.source.slot);
            if (Selectable(label,&light==selected)) { tools.selected=light.source; selected=&light; }
        }
        EndCombo();
    }
    if (!selected) return;
    Text("XY %.1f, %.1f   Height %.1f",selected->position.x,selected->position.y,selected->height);
    const auto c=selected->color;
    Text("Linear RGB       %.2f  %.2f  %.2f",c.r,c.g,c.b);
    Text("Radius %.1f   Intensity %.2f",selected->radius,selected->intensity);
    TextWrapped("Light shows linear illumination mapped for display.");
}
void shadowControls(const Display& d,Commands& cmd,ToolState& tools,const History& history,
                    const std::string& reason) {
    shadowEnable(d,cmd);
    SameLine();
    if (Button(d.view==View::Shadow ? "Final (4)" : "ShadowMask (7)"))
        cmd.view=d.view==View::Shadow ? View::Final : View::Shadow;
    int budget=int(d.shadowLights);
    SetNextItemWidth(-105);
    if (SliderInt("Caster budget",&budget,1,int(Display::maxShadowLights),"%d",ImGuiSliderFlags_AlwaysClamp))
        cmd.shadowLights=unsigned(budget);
    Checkbox("Occluder edges",&tools.occluderEdges); SameLine();
    Checkbox("Shadow geometry",&tools.shadowGeometry);
    Checkbox("Buffer previews",&tools.previews);
    if (!history.cpu.empty() && history.cpu.back().info.display.shadows) {
        const auto& sample=history.cpu.back();
        const auto& stats=sample.stats;
        Text("CPU frame #%llu   us",U64(sample.info.id));
        Text("Batch %.1f   inspect %.1f",stats.cpuUs[ShadowBatch],stats.cpuUs[ShadowInspect]);
        Text("Mask draws %u   light %u   inspect %u",stats.maskDraws,stats.lightDraws,stats.inspectDraws);
    }
    if (tools.timing && !history.gpu.empty() && history.gpu.back().us[1+ShadowBatch]) {
        const auto& sample=history.gpu.back();
        Text("GPU #%llu   batch %.1f us",U64(sample.info.id),*sample.us[1+ShadowBatch]);
    } else TextWrapped("GPU shadow timing N/A (%s)",tools.timing ? reason.c_str() : "off");
}
void shadowDetails(ToolState& tools,const Picture& picture,const ShadowCasters& casters,
                   const FrameInfo& frame) {
    Text("Casters %u   skipped %u",casters.count,casters.skipped);
    Text("Visual-only walls %u   edges %u",picture.occluders.size(),picture.occluders.edgeCount());
    auto label=[](LabHandle id,char (&out)[64]) {
        std::snprintf(out,sizeof(out),"%s %u / gen %llu",name(id.kind),id.slot,U64(id.generation));
    };
    char selected[64]="N/A";
    if (tools.shadowSource) label(*tools.shadowSource,selected);
    SetNextItemWidth(-65);
    if (BeginCombo("Caster",tools.followPlayer && tools.shadowSource ? "Player (default)" : selected)) {
        for (unsigned i=0;i<casters.count;++i) {
            const auto& id=picture.lights.begin()[casters.indices[i]].source;
            char item[64];
            label(id,item);
            if (Selectable(item,tools.shadowSource && *tools.shadowSource==id)) {
                tools.followPlayer=id.kind==ObjectKind::Player;
                tools.shadowSource=id;
            }
        }
        EndCombo();
    }
    Text("Frame %llu  Tick %llu",U64(frame.id),U64(frame.tick));
    Text("Epoch %llu  FB generation %llu",U64(frame.epoch),U64(frame.generation));
    TextDisabled("ShadowMask: white visible, black blocked");
}
void objectList(const Neon& n,std::uint64_t epoch,ToolState& tools,char (&filter)[64]) {
    const auto list=objects(n,epoch);
    std::array<unsigned,4> count{};
    for (auto& o:list) ++count[unsigned(o.id.kind)];
    Text("Player %u   Relay %u   Bolt %u   Burst %u",count[3],count[0],count[1],count[2]);
    SetNextItemWidth(-1); InputTextWithHint("##filter","Filter objects",filter,sizeof(filter),ImGuiInputTextFlags_EnterReturnsTrue);
    if (BeginChild("objects",{0,100},ImGuiChildFlags_Borders)) {
        for (const auto& o:list) {
            char label[40]; std::snprintf(label,sizeof(label),"%s %u",name(o.id.kind),o.id.slot);
            if (!std::strstr(label,filter)) continue;
            if (Selectable(label,tools.selected && *tools.selected==o.id)) tools.selected=o.id;
        }
    }
    EndChild();
    if (!tools.selected) { TextDisabled("Select from the scene or list"); return; }
    auto o=resolve(n,epoch,*tools.selected);
    if (!o) return;
    TextColored({1,.72f,.25f,1},"%s %u",name(o->id.kind),o->id.slot);
    Text("Epoch %llu   Generation %llu",U64(epoch),U64(o->id.generation));
    Text("Position %.1f, %.1f",o->position.x,o->position.y);
    if (o->age>=0) Text("Age %d / %d ticks",o->age,o->lifetime);
    if (o->id.kind==ObjectKind::Bolt) {
        Text("Start %.1f, %.1f",o->start.x,o->start.y);
        Text("End   %.1f, %.1f",o->end.x,o->end.y);
    }
    if (o->id.kind==ObjectKind::Burst) Text("Angle %.3f rad",o->angle);
    Text("Snapshot tick %llu   Collider: none",U64(n.tick));
}
template<class Window,class Value>
void row(const char* label,const Window& samples,Value value) {
    std::vector<double> values;
    for (const auto& s:samples) if (auto v=value(s)) values.push_back(*v);
    if (values.empty()) { TextDisabled("%-10s  --   --   --   0",label); return; }
    std::sort(values.begin(),values.end());
    auto q=[&](double p){return values[std::size_t(std::ceil(p*values.size()))-1];};
    Text("%-10s %4.0f %4.0f %4.0f %3zu",label,q(.5),q(.95),q(.99),values.size());
    if (IsItemHovered()) {
        auto last=value(samples.back());
        if (last) SetTooltip("Latest %.3f us",*last); else SetTooltip("Skipped in latest frame");
    }
}
void performance(const History& history,const tiny2d::QueryCounts& counts,const std::string& reason,
                 const std::string& renderer,const std::string& version) {
    TextWrapped("%s | GL %s",renderer.c_str(),version.c_str());
    TextWrapped("Timestamp: %s",reason.c_str());
    TextDisabled("Query records since launch");
    Text("Requested %llu   Complete %llu",U64(counts.requested),U64(counts.completed));
    Text("Pending %llu   Skipped %llu   Invalid %llu",U64(counts.pending),U64(counts.skipped),U64(counts.invalid));
    if (history.cpu.empty()) return;
    const auto& c=history.cpu.back(); const auto& s=c.stats;
    Text("CPU #%llu   tick %llu   %.1f us",U64(c.info.id),U64(c.info.tick),c.total);
    Text("Window %dx%d   FB %dx%d",c.info.logical.width,c.info.logical.height,c.info.pixels.width,c.info.pixels.height);
    Text("%u passes / %llu draws / %llu world sprites",s.passes,U64(s.draws),U64(s.worldSprites));
    Text("GUI %u vertices / %u indices",s.guiVertices,s.guiIndices);
    Text("Batch payload %llu B",U64(s.bytes));
    Text("GUI V/I %llu / %llu B",U64(s.vertexBytes),U64(s.indexBytes));
    Text("Font upload %llu B",U64(s.fontUpload));
    TextWrapped("Attachment bytes: HDR %llu, mask %llu, thumbnails %llu, font %llu",
                U64(c.hdrBytes),U64(c.shadowBytes),U64(c.previewBytes),U64(s.fontBytes));
    if (c.materialBytes) Text("Normal tile %llu B",U64(c.materialBytes));
    SeparatorText("CPU submission / us");
    TextDisabled("stage       p50  p95  p99   n");
    auto cpu=[&](const char* n,double CpuSample::*field){row(n,history.cpu,[&](const auto& v){return std::optional<double>(v.*field);});};
    cpu("frame",&CpuSample::total); cpu("update",&CpuSample::update); cpu("prepare",&CpuSample::prepare);
    auto measured=[&](const char* n,double CpuSample::*field) {
        row(n,history.cpu,[&](const CpuSample& v)->std::optional<double>{
            if (v.*field>0) return v.*field;
            return {};
        });
    };
    measured("GUI build",&CpuSample::build); measured("query poll",&CpuSample::poll);
    measured("light data",&CpuSample::lightExtract);
    measured("tile build",&CpuSample::normalPrepare);
    for (unsigned i=0;i<PassCount;++i) {
        char label[40]; std::snprintf(label,sizeof(label),"%s[%u]",passNames[i],s.passDraws[i]);
        row(label,history.cpu,[&](const auto& v)->std::optional<double>{
            if (v.stats.mask & (1u<<i)) return v.stats.cpuUs[i]; return {};
        });
    }
    SeparatorText("GPU timestamps / us");
    if (!history.gpu.empty()) {
        const auto& g=history.gpu.back();
        Text("GPU #%llu  +%llu frames",U64(g.info.id),U64(g.received-g.info.id));
        for (unsigned i=0;i<=PassCount;++i)
            row(i?passNames[i-1]:"render",history.gpu,[&](const auto& v){return v.us[i];});
    } else TextDisabled("No completed samples");
}
}
void ToolState::resolveShadow(const LightList& lights,const ShadowCasters& casters,std::uint64_t epoch) {
    if (followPlayer) shadowSource=LabHandle{epoch,ObjectKind::Player,0,1};
    if (shadowSource && !casters.find(lights,*shadowSource)) shadowSource.reset();
}
Inspector::Inspector(tiny2d::Window& w,bool finite):window_(w.nativeHandle()),finite_(finite) {
    requireOwner();
    renderer_=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    version_=reinterpret_cast<const char*>(glGetString(GL_VERSION));
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    auto& io=ImGui::GetIO(); io.IniFilename=nullptr;
    if (finite) {
        io.ConfigFlags |= ImGuiConfigFlags_NoKeyboard|ImGuiConfigFlags_NoMouse|ImGuiConfigFlags_NoMouseCursorChange;
        io.SetAppAcceptingEvents(false);
    }
    auto& style=ImGui::GetStyle();
    style.WindowRounding=6; style.FrameRounding=3; style.WindowPadding={9,7};
    style.Colors[ImGuiCol_WindowBg]={.035f,.055f,.071f,.97f};
    style.Colors[ImGuiCol_TitleBgActive]={.06f,.24f,.26f,1};
    style.Colors[ImGuiCol_CheckMark]={.30f,.91f,.81f,1};
    style.Colors[ImGuiCol_SliderGrab]={.96f,.63f,.27f,1};
    style.FontScaleDpi=ImGui_ImplGlfw_GetContentScaleForWindow(window_);
    style.ScaleAllSizes(style.FontScaleDpi);
    if (!ImGui_ImplGlfw_InitForOpenGL(window_,true)) {
        ImGui::DestroyContext(); throw std::runtime_error("Inspector GLFW initialization failed");
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
        ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext(); throw std::runtime_error("Inspector GL initialization failed");
    }
    try {
        if (!ImGui_ImplOpenGL3_CreateDeviceObjects()) throw std::runtime_error("Inspector shader initialization failed");
        tiny2d::detail::checkGL("Inspector device initialization");
    } catch (...) {
        ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
        throw;
    }
    if (!finite_) {
        io.UserData=this;
        // The official backend restores the original Window callback on shutdown.
        glfwSetWindowFocusCallback(window_,focusChanged);
    }
}
void Inspector::focusChanged(GLFWwindow* window,int focused) {
    auto& io=ImGui::GetIO();
    auto& inspector=*static_cast<Inspector*>(io.UserData);
    // Cancel both undelivered transitions and ImGui's sampled down state at
    // the callback boundary. The rest of this poll belongs to the new cycle.
    cancelPendingInput(io);
    inspector.focusGate_=true;
    inspector.mouseBlocked_=true;
    inspector.f2Down_=false;
    inspector.freshFocus_=focused!=0;
    inspector.restoreCursor_=focused!=0;
    auto& held=inspector.heldAtLoss_;
    if (!focused) {
        // GLFW synthesizes releases *after* its focus-loss callback returns.
        // Keep unresolved keys across successive focus changes.
        for (int key=GLFW_KEY_SPACE;key<=GLFW_KEY_LAST;++key)
            if (glfwGetKey(window,key)==GLFW_PRESS) held.set(key);
    }
    glfwSetKeyCallback(window,focused && held.any() ? keyAfterFocus : ImGui_ImplGlfw_KeyCallback);
    ImGui_ImplGlfw_WindowFocusCallback(window,focused); // Chains Window's focus-loss latch.
}
void Inspector::keyAfterFocus(GLFWwindow* window,int key,int scancode,int action,int mods) {
    if (action==GLFW_PRESS && key>=0 && key<=GLFW_KEY_LAST) {
        auto& held=static_cast<Inspector*>(ImGui::GetIO().UserData)->heldAtLoss_;
        if (held[key]) {
            // The released GLFW cache labels the first native auto-repeat PRESS.
            // A genuine fresh press, including one released while away, passes through.
            if (cocoaKeyRepeats(window,scancode)) action=GLFW_REPEAT;
            held.reset(key);
            if (held.none()) glfwSetKeyCallback(window,ImGui_ImplGlfw_KeyCallback);
        }
    }
    ImGui_ImplGlfw_KeyCallback(window,key,scancode,action,mods);
}
void Inspector::requireOwner() const {
    tiny2d::detail::requireContext("Lab Inspector");
    if (glfwGetCurrentContext()!=window_) throw std::logic_error("Inspector on foreign context");
}
Inspector::~Inspector() {
    requireOwner(); discard(); ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
}
void Inspector::discard() {
    if (active_) ImGui::EndFrame();
    active_=false; ready_=false;
}
void Inspector::suspend() {
    discard();
    cancelPendingInput(ImGui::GetIO());
    focusGate_=true; freshFocus_=false; mouseBlocked_=true; restoreCursor_=true;
}
RoutedInput Inspector::build(ToolState& tools,const Neon& n,std::uint64_t epoch,const Display& display,
        bool paused,const History& hist,const tiny2d::QueryCounts& counts,const std::string& reason,bool focused) {
    requireOwner(); discard();
    if (restoreCursor_ && focused && !finite_) {
        restoreCursor_=false;
        if (glfwGetWindowAttrib(window_,GLFW_HOVERED)) {
            double x,y; glfwGetCursorPos(window_,&x,&y);
            ImGui::GetIO().AddMousePosEvent(float(x),float(y));
        }
    }
    ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame();
    if (finite_) ImGui::GetIO().DeltaTime=1.f/60;
    ImGui::NewFrame(); active_=true;
    auto& io=ImGui::GetIO();
    RoutedInput input;
    for (unsigned i=0;i<KeyCount;++i) input.keys[i]=rawDown(keys[i]);
    bool f2=rawDown(ImGuiKey_F2);
    input.freshFocus=focused && freshFocus_;
    freshFocus_=false;
    bool toggle=focused && (!focusGate_ || input.freshFocus) && f2 && !f2Down_ && !finite_;
    if (toggle) tools.visible=!tools.visible;
    f2Down_=f2;
    bool transition=toggle || first_ || focusGate_;
    bool gateKeyboard=toggle || first_ || (focusGate_ && !input.freshFocus);
    const bool startOnLighting=first_ && display.lighting && !display.shadows;
    const bool startOnShadows=first_ && display.shadows;
    lightingTab_=shadowTab_=false;
    first_=false; focusGate_=!focused;
    if (tools.visible) {
        const float w=std::min(292.f,std::max(1.f,io.DisplaySize.x-8));
        const float top=std::min(55.f,io.DisplaySize.y*.12f);
        SetNextWindowPos({std::max(0.f,io.DisplaySize.x-w-6),top},ImGuiCond_FirstUseEver);
        SetNextWindowSize({w,std::max(48.f,io.DisplaySize.y-top-(tools.previews && !display.lighting?150.f:56.f))},ImGuiCond_FirstUseEver);
        SetNextWindowSizeConstraints({std::min(180.f,w),40},{io.DisplaySize.x,std::max(40.f,io.DisplaySize.y)});
        if (Begin("Neon Inspector",nullptr,finite_?ImGuiWindowFlags_NoInputs:0)) {
            if (BeginTabBar("panels")) {
                if (BeginTabItem("Controls")) {
                    controls(display,paused,input.commands,tools); EndTabItem();
                }
                if (BeginTabItem("Objects")) { objectList(n,epoch,tools,filter_); EndTabItem(); }
                if (BeginTabItem("Timing")) { performance(hist,counts,tools.timing?reason:"off",renderer_,version_); EndTabItem(); }
                if (BeginTabItem("Lighting",nullptr,startOnLighting?ImGuiTabItemFlags_SetSelected:0)) {
                    lightingTab_=true;
                    lightingControls(display,input.commands,tools);
                    EndTabItem();
                }
                if (display.shadows && BeginTabItem("Shadows",nullptr,startOnShadows?ImGuiTabItemFlags_SetSelected:0)) {
                    shadowTab_=true;
                    shadowControls(display,input.commands,tools,hist,reason);
                    EndTabItem();
                }
                EndTabBar();
            }
            if (!lightingTab_ && !shadowTab_ && display.view!=View::Final && display.view!=View::Scene)
                TextWrapped("Use the object list in this buffer view.");
            if (!lightingTab_ && !shadowTab_ && !hist.cpu.empty()) {
                auto& c=hist.cpu.back();
                TextDisabled("CPU #%llu  %.0f us  %llu draws",U64(c.info.id),c.total,U64(c.stats.draws));
                if (!tools.timing) TextDisabled("GPU timing off");
                else if (!hist.gpu.empty()) {
                    auto& g=hist.gpu.back();
                    TextDisabled("GPU #%llu  %.0f us  (+%llu)",U64(g.info.id),g.us[0].value_or(0),U64(g.received-g.info.id));
                } else TextWrapped("GPU %s",reason.c_str());
            }
        }
        End();
    }
    input.captureKeyboard=gateKeyboard || (tools.visible && (io.WantCaptureKeyboard || ImGui::IsAnyItemActive()));
    const bool captured=transition || !focused || (tools.visible && (io.WantCaptureMouse || IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopup)));
    if (!io.MouseDown[0]) mouseBlocked_=false;
    if (captured && io.MouseDown[0]) mouseBlocked_=true;
    if (!finite_ && !captured && !mouseBlocked_ && io.MouseClicked[0])
        input.click=tiny2d::Vector2{io.MouseClickedPos[0].x,io.MouseClickedPos[0].y};
    return input;
}
unsigned Inspector::finish(ToolState& tools,Pipeline& pipe,const FrameInfo& info,
                           const Picture& picture,const ShadowCasters& casters) {
    requireOwner();
    unsigned mask=0;
    const auto frame=info.id,tick=info.tick;
    const bool lit=info.display.lighting,normals=info.display.normalMapping;
    if (info.display.shadows) tools.resolveShadow(picture.lights,casters,info.epoch);
    if ((lightingTab_ && lit) || (shadowTab_ && info.display.shadows)) {
        // Finish after fixed updates. Details and texture views describe the
        // exact light payload submitted in this frame.
        if (Begin("Neon Inspector",nullptr,finite_?ImGuiWindowFlags_NoInputs:0)) {
            if (shadowTab_) shadowDetails(tools,picture,casters,info);
            else {
                if (normals) {
                    const auto size=pipe.normalSize();
                    Text("Normal XYZ   RGBA16F %dx%d",size.width,size.height);
                }
                lightDetails(tools,picture.lights,tick);
            }
        }
        End();
    }
    if (tools.visible && tools.previews) {
        auto size=GetIO().DisplaySize;
        const float width=std::clamp(size.x-310.f,std::min(260.f,std::max(1.f,size.x-12)),720.f);
        const float height=std::min(147.f,std::max(60.f,size.y*.36f));
        SetNextWindowPos({6,std::max(0.f,size.y-45.f-height)},ImGuiCond_FirstUseEver);
        SetNextWindowSize({width,height},ImGuiCond_Always);
        if (Begin("Buffers",nullptr,(finite_?ImGuiWindowFlags_NoInputs:0))) {
            const bool shadow=info.display.shadows && tools.shadowSource.has_value();
            if (info.display.shadows) {
                TextDisabled("Frame %llu  Tick %llu  Epoch %llu  FB gen %llu",U64(frame),U64(tick),U64(info.epoch),U64(info.generation));
                if (shadow) {
                    const auto id=*tools.shadowSource;
                    TextDisabled("Mask: %s %u gen %llu%s",name(id.kind),id.slot,U64(id.generation),
                                 tools.followPlayer ? " (default)" : "");
                } else TextDisabled("ShadowMask N/A: select an active caster");
            }
            else if (normals) TextDisabled("Frame %llu  Tick %llu  |  Normal XYZ: (n+1)/2",U64(frame),U64(tick));
            else if (lit) TextDisabled("Frame %llu  Tick %llu  |  Reinhard + sRGB",U64(frame),U64(tick));
            else TextDisabled("Frame %llu  Tick %llu  |  Reinhard + sRGB  |  Final: scene composite",U64(frame),U64(tick));
            std::array<PreviewView,7> views{};
            const unsigned count=4+unsigned(lit)+unsigned(normals)+unsigned(shadow);
            try {
                const auto legacy=pipe.previews();
                std::copy(legacy.begin(),legacy.end(),views.begin());
                if (lit) views[4]=pipe.lightPreview();
                if (normals) views[5]=pipe.normalPreview();
                if (shadow) views[6]=pipe.shadowPreview();
            }
            catch (const std::exception& error) {
                End(); discard(); throw PreviewAllocationError(error.what());
            } catch (...) { End(); discard(); throw; }
            const float cell=std::max(2.f,(GetContentRegionAvail().x-(count-1)*GetStyle().ItemSpacing.x)/count);
            for (unsigned i=0;i<views.size();++i) {
                const auto& v=views[i];
                if (!v.texture) continue;
                if (i) SameLine();
                BeginGroup();
                if (lit) TextUnformatted(name(static_cast<View>(i)));
                else Text("%s %dx%d",name(static_cast<View>(i)),v.source.width,v.source.height);
                const float scale=std::min((cell-1)/v.thumbnail.width,56.f/v.thumbnail.height);
                Image(ImTextureRef(ImTextureID(v.texture)),{v.thumbnail.width*scale,v.thumbnail.height*scale},{0,1},{1,0});
                if (IsItemVisible()) mask |= 1u<<i;
                if (!lit) TextDisabled("RGBA8 %dx%d",v.thumbnail.width,v.thumbnail.height);
                else if (IsItemHovered())
                    SetTooltip("Source %dx%d; RGBA8 preview %dx%d",v.source.width,v.source.height,v.thumbnail.width,v.thumbnail.height);
                EndGroup();
            }
            if (normals) {
                const auto& v=views[unsigned(View::Normal)];
                TextDisabled("Normal %dx%d -> RGBA8 %dx%d",v.source.width,v.source.height,v.thumbnail.width,v.thumbnail.height);
            }
        }
        End();
    }
    if (tools.visible) { ImGui::Render(); ready_=true; } else ImGui::EndFrame();
    active_=false;
    return mask;
}
void Inspector::submit(Stats& s) {
    requireOwner();
    if (!ready_) throw std::logic_error("Inspector draw data unavailable");
    ready_=false;
    auto* data=ImGui::GetDrawData();
    // The official backend restores alignment/row length but assumes CPU pointers
    // and zero pixel offsets for atlas updates.
    GLint pbo,skipRows,skipPixels;
    struct RestoreUnpack {
        GLint &buffer,&rows,&pixels;
        ~RestoreUnpack() {
            glPixelStorei(GL_UNPACK_SKIP_ROWS,rows); glPixelStorei(GL_UNPACK_SKIP_PIXELS,pixels);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER,buffer);
        }
    } restore{pbo,skipRows,skipPixels};
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&pbo);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS,&skipRows); glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skipPixels);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS,0); glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
    if (data->Textures) for (auto* tex:*data->Textures) {
        if (tex->Status==ImTextureStatus_WantCreate) s.fontUpload+=tex->Width*tex->Height*4ull;
        if (tex->Status==ImTextureStatus_WantUpdates)
            for (auto rect:tex->Updates) s.fontUpload+=4ull*rect.w*rect.h;
    }
    ImGui_ImplOpenGL3_Stats actual;
    ImGui_ImplOpenGL3_RenderDrawData(data,&actual);
    s.guiVertices=data->TotalVtxCount; s.guiIndices=data->TotalIdxCount;
    s.vertexBytes=actual.VertexBytes; s.indexBytes=actual.IndexBytes; s.draws+=actual.Draws;
    if (data->Textures) for (auto* tex:*data->Textures)
        if (tex->TexID!=ImTextureID_Invalid) s.fontBytes+=tex->Width*tex->Height*4ull;
    atlasBytes_=s.fontBytes;
}
}
