#include "Inspector.h"
#include "OpenGL.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace lab {
namespace {
using namespace ImGui;
using U64=unsigned long long;
constexpr ImGuiKey keys[]={ImGuiKey_A,ImGuiKey_D,ImGuiKey_W,ImGuiKey_S,ImGuiKey_Space,ImGuiKey_B,
    ImGuiKey_1,ImGuiKey_2,ImGuiKey_3,ImGuiKey_4,ImGuiKey_LeftBracket,ImGuiKey_RightBracket,
    ImGuiKey_P,ImGuiKey_Period,ImGuiKey_R,ImGuiKey_F1,ImGuiKey_Escape};
static_assert(std::size(keys)==KeyCount);
bool rawDown(ImGuiKey k) { return GetIO().KeysData[k-ImGuiKey_NamedKey_BEGIN].Down; }
void controls(const Display& d,bool paused,Commands& cmd,ToolState& tools) {
    bool bloom=d.bloom,hud=d.hud;
    if (Checkbox("Bloom",&bloom)) cmd.edges.set(Bloom);
    SameLine(); if (Checkbox("HUD",&hud)) cmd.edges.set(Hud);
    int strength=d.strength;
    char gain[24]; std::snprintf(gain,sizeof(gain),"%.1f",strength/10.);
    SetNextItemWidth(-74);
    if (SliderInt("Strength",&strength,0,30,gain,ImGuiSliderFlags_AlwaysClamp|ImGuiSliderFlags_NoInput))
        cmd.strength=strength;
    int view=int(d.view);
    SetNextItemWidth(-74);
    if (Combo("View",&view,"Scene\0Bright\0Blur\0Final\0")) cmd.view=static_cast<View>(view);
    if (Button(paused?"Resume":"Pause")) cmd.edges.set(Pause);
    SameLine(); BeginDisabled(!paused);
    if (Button("Step 1 tick")) cmd.edges.set(Step);
    EndDisabled(); SameLine(); if (Button("Reset")) cmd.edges.set(Reset);
    Separator();
    Checkbox("PickBounds",&tools.bounds); SameLine(); Checkbox("Outline",&tools.outline);
    Checkbox("Buffers",&tools.previews); SameLine(); Checkbox("GPU timing",&tools.timing);
    TextDisabled("Threshold 1   Gaussian r8 / 3.2");
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
    TextWrapped("Attachment bytes: HDR %llu, thumbnails %llu, font %llu",U64(c.hdrBytes),U64(c.previewBytes),U64(s.fontBytes));
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
void Inspector::suspend() { discard(); focusGate_=true; mouseBlocked_=true; }
RoutedInput Inspector::build(ToolState& tools,const Neon& n,std::uint64_t epoch,const Display& display,
        bool paused,const History& hist,const tiny2d::QueryCounts& counts,const std::string& reason,bool focused) {
    requireOwner(); discard();
    ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame();
    if (finite_) ImGui::GetIO().DeltaTime=1.f/60;
    ImGui::NewFrame(); active_=true;
    auto& io=ImGui::GetIO();
    RoutedInput input;
    for (unsigned i=0;i<KeyCount;++i) input.keys[i]=rawDown(keys[i]);
    bool f2=rawDown(ImGuiKey_F2);
    bool toggle=focused && !focusGate_ && f2 && !f2Down_ && !finite_;
    if (toggle) tools.visible=!tools.visible;
    f2Down_=f2;
    bool transition=toggle || first_ || focusGate_;
    first_=false; focusGate_=!focused;
    if (tools.visible) {
        const float w=std::min(292.f,std::max(1.f,io.DisplaySize.x-8));
        const float top=std::min(55.f,io.DisplaySize.y*.12f);
        SetNextWindowPos({std::max(0.f,io.DisplaySize.x-w-6),top},ImGuiCond_FirstUseEver);
        SetNextWindowSize({w,std::max(48.f,io.DisplaySize.y-top-(tools.previews?150.f:56.f))},ImGuiCond_FirstUseEver);
        SetNextWindowSizeConstraints({std::min(180.f,w),40},{io.DisplaySize.x,std::max(40.f,io.DisplaySize.y)});
        if (Begin("Neon Inspector",nullptr,finite_?ImGuiWindowFlags_NoInputs:0)) {
            if (BeginTabBar("panels")) {
                if (BeginTabItem("Controls")) {
                    controls(display,paused,input.commands,tools); EndTabItem();
                }
                if (BeginTabItem("Objects")) { objectList(n,epoch,tools,filter_); EndTabItem(); }
                if (BeginTabItem("Timing")) { performance(hist,counts,tools.timing?reason:"off",renderer_,version_); EndTabItem(); }
                EndTabBar();
            }
            if (display.view==View::Bright || display.view==View::Blur) TextWrapped("Use the object list in this buffer view.");
            if (!hist.cpu.empty()) {
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
    input.captureKeyboard=transition || (tools.visible && (io.WantCaptureKeyboard || ImGui::IsAnyItemActive()));
    const bool captured=transition || !focused || (tools.visible && (io.WantCaptureMouse || IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopup)));
    if (!io.MouseDown[0]) mouseBlocked_=false;
    if (captured && io.MouseDown[0]) mouseBlocked_=true;
    if (!finite_ && !captured && !mouseBlocked_ && io.MouseClicked[0])
        input.click=tiny2d::Vector2{io.MouseClickedPos[0].x,io.MouseClickedPos[0].y};
    return input;
}
unsigned Inspector::finish(const ToolState& tools,Pipeline& pipe,std::uint64_t frame,std::uint64_t tick) {
    requireOwner();
    unsigned mask=0;
    if (tools.visible && tools.previews) {
        auto size=GetIO().DisplaySize;
        const float width=std::clamp(size.x-310.f,std::min(260.f,std::max(1.f,size.x-12)),720.f);
        const float height=std::min(147.f,std::max(60.f,size.y*.36f));
        SetNextWindowPos({6,std::max(0.f,size.y-45.f-height)},ImGuiCond_FirstUseEver);
        SetNextWindowSize({width,height},ImGuiCond_Always);
        if (Begin("Buffers",nullptr,(finite_?ImGuiWindowFlags_NoInputs:0))) {
            TextDisabled("Frame %llu  Tick %llu  |  Reinhard + sRGB  |  Final: scene composite",U64(frame),U64(tick));
            std::array<PreviewView,4> views;
            try { views=pipe.previews(); }
            catch (...) { End(); discard(); throw; }
            const float cell=std::max(1.f,(GetContentRegionAvail().x-3*GetStyle().ItemSpacing.x)/4);
            for (unsigned i=0;i<4;++i) {
                const auto& v=views[i]; if (i) SameLine();
                BeginGroup();
                Text("%s %dx%d",name(static_cast<View>(i)),v.source.width,v.source.height);
                const float scale=std::min((cell-1)/v.thumbnail.width,56.f/v.thumbnail.height);
                Image(ImTextureRef(ImTextureID(v.texture)),{v.thumbnail.width*scale,v.thumbnail.height*scale},{0,1},{1,0});
                if (IsItemVisible()) mask |= 1u<<i;
                TextDisabled("RGBA8 %dx%d",v.thumbnail.width,v.thumbnail.height);
                EndGroup();
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
