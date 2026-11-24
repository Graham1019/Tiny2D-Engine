#include "ShowcaseApp.h"
#include "ShowcaseProcess.h"
#include "Capture.h"
#include "Inspector.h"
#include "ShaderAssets.h"
#include "tiny2d/Texture2D.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace lab {
namespace fs=std::filesystem;
namespace {
using namespace ImGui;
const ImVec4 accent{.40f,.89f,.77f,1.f};
const ImVec4 muted{.67f,.72f,.74f,1.f};

struct ExhibitPreview {
    std::optional<ExhibitId> selected;
    std::unique_ptr<tiny2d::Texture2D> texture;
    std::string error;
    void load(const Exhibit& entry) {
        if (selected==entry.id) return;
        selected=entry.id;
        texture.reset();
        error.clear();
        try {
            const auto file=assetPath((std::string("showcase/")+entry.preview).c_str());
            texture=std::make_unique<tiny2d::Texture2D>(tiny2d::ImageLoader::loadPNG(file));
        } catch (const std::exception& failure) { error=failure.what(); }
    }
    void draw(float width,float height) const {
        if (!texture) { TextWrapped("%s",error.c_str()); return; }
        const float source=float(texture->width())/texture->height(), target=width/height;
        const auto u=std::min(1.f,target/source),v=std::min(1.f,source/target);
        const auto p=GetCursorScreenPos();
        GetWindowDrawList()->AddImageRounded(ImTextureRef(ImTextureID(texture->nativeHandle())),p,
            {p.x+width,p.y+height},{(1-u)*.5f,(1-v)*.5f},{(1+u)*.5f,(1+v)*.5f},IM_COL32_WHITE,6);
        Dummy({width,height});
    }
};

void styleShowcase() {
    auto& style=GetStyle();
    style.WindowPadding={20,16};
    style.FramePadding={10,6};
    style.ItemSpacing={8,6};
    style.Colors[ImGuiCol_WindowBg]={.047f,.065f,.077f,1.f};
    style.Colors[ImGuiCol_Text]={.94f,.95f,.94f,1.f};
    style.Colors[ImGuiCol_Header]={.10f,.27f,.26f,1.f};
    style.Colors[ImGuiCol_HeaderHovered]={.13f,.32f,.30f,1.f};
    style.Colors[ImGuiCol_HeaderActive]={.16f,.36f,.33f,1.f};
    style.Colors[ImGuiCol_Separator]={.18f,.23f,.25f,1.f};
    style.Colors[ImGuiCol_Button]={.16f,.42f,.37f,1.f};
    style.Colors[ImGuiCol_ButtonHovered]={.18f,.49f,.43f,1.f};
    style.Colors[ImGuiCol_ButtonActive]={.12f,.37f,.32f,1.f};
    // Use the OS font locally when present. The bundled ImGui font is always available.
    const auto font="/System/Library/Fonts/SFNS.ttf";
    if (fs::is_regular_file(font)) {
        if (auto* system=GetIO().Fonts->AddFontFromFileTTF(font,16.f)) GetIO().FontDefault=system;
    }
}

bool panel(ExhibitId& selected,std::optional<ExhibitId> active,const std::string& status,ExhibitPreview& preview) {
    const auto display=GetIO().DisplaySize;
    SetNextWindowPos({0,0});
    SetNextWindowSize(display);
    Begin("Tiny2D Showcase",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|
                                    ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBringToFrontOnFocus);
    PushFont(nullptr,27.f);
    TextUnformatted("Tiny2D Showcase");
    PopFont();
    Separator();
    const bool wide=display.x>=650;
    if (wide) {
        const float sidebar=std::clamp(display.x*.20f,153.f,220.f);
        BeginChild("Exhibits",{sidebar,0},ImGuiChildFlags_None);
        TextColored(muted,"EXHIBITS");
        for (const auto& entry:exhibits()) {
            if (Selectable(entry.title,selected==entry.id,0,{0,28})) selected=entry.id;
        }
        EndChild();
        SameLine(0,18);
    } else {
        SetNextItemWidth(-1);
        if (BeginCombo("##exhibits",exhibits()[std::size_t(selected)].title)) {
            for (const auto& entry:exhibits()) if (Selectable(entry.title,selected==entry.id)) selected=entry.id;
            EndCombo();
        }
    }
    BeginChild("Exhibit detail",{0,0});
    const auto& entry=exhibits()[std::size_t(selected)];
    preview.load(entry);
    const auto area=GetContentRegionAvail();
    const float width=std::max(1.f,area.x);
    const float height=std::max(70.f,std::min(width*.56f,area.y-172.f));
    preview.draw(width,height);
    PushFont(nullptr,23.f);
    TextUnformatted(entry.title);
    PopFont();
    TextWrapped("%s",entry.feature);
    PushStyleColor(ImGuiCol_Text,muted);
    TextWrapped("%s",entry.controls);
    PopStyleColor();
    Dummy({1,2});
    BeginDisabled(active.has_value());
    const bool open=Button("Open exhibit",{125.f,30.f});
    EndDisabled();
    if (active) {
        TextColored(accent,"Running: %s",exhibits()[std::size_t(*active)].title);
    } else if (!status.empty()) TextWrapped("%s",status.c_str());
    TextColored(muted,"Esc or close the exhibit window to return here.");
    EndChild();
    End();
    return open;
}
}

void runShowcase(const Options& options) {
    const auto actualLab=tiny2dExecutablePath();
    const auto visits=options.showcaseTour.empty()?std::vector<ExhibitId>{}:tourVisits(options.showcaseTour);
    const bool tour=!visits.empty(),finite=tour || options.frames.has_value();
    ExhibitId selected=findExhibit(options.showcaseExhibit).id;
    fs::path output;
    if (!options.showcaseOutput.empty()) {
        output=fs::absolute(options.showcaseOutput);
        if (!fs::create_directory(output)) throw std::runtime_error("showcase output directory already exists: "+output.string());
    }
    std::ofstream metrics;
    if (!options.metrics.empty()) {
        metrics.exceptions(std::ios::badbit|std::ios::failbit);
        metrics.open(options.metrics);
        metrics<<"frame,logical_w,logical_h,fb_w,fb_h,selected,child_running,gui_build_us,cpu_submit_us,gui_vertices,gui_indices\n";
        metrics<<std::fixed<<std::setprecision(3);
    }
    tiny2d::Window window(options.width,options.height,"Tiny2D Showcase");
    if (finite) glfwSwapInterval(0);
    Inspector gui(window,finite);
    styleShowcase();
    ToolState tools{true,false,false};
    ExhibitPreview preview;
    ExhibitProcess process;
    std::optional<ExhibitId> active;
    std::string status;
    std::size_t launched=0,reaped=0,frames=0;
    Clock::time_point started;
    std::optional<Clock::time_point> unavailable;
    while (!window.shouldClose()) {
        window.pollEvents();
        if (window.shouldClose()) break;
        bool returned=false;
        if (const auto result=process.poll()) {
            const auto& entry=exhibits()[std::size_t(*active)];
            std::cout<<"showcase_exit exhibit="<<entry.key<<" code="<<result->code<<" signal="<<result->signal<<std::endl;
            ++reaped;
            returned=true;
            status=result->code ? std::string(entry.title)+" exited with code "+std::to_string(result->code)+"."
                                : std::string(entry.title)+" closed.";
            active.reset();
            if (tour && result->code) throw std::runtime_error(status);
            if (!finite) {
                glfwRestoreWindow(window.nativeHandle());
                glfwFocusWindow(window.nativeHandle());
            }
        }
        if (tour && active && Clock::now()-started>std::chrono::seconds(120))
            throw std::runtime_error("showcase child timed out: "+std::string(exhibits()[std::size_t(*active)].key));
        const auto pixels=window.framebufferSize(),logical=window.size();
        if (!pixels.width || !pixels.height || !logical.width || !logical.height || window.iconified()) {
            if (!unavailable) unavailable=Clock::now();
            if (finite && Clock::now()-*unavailable>std::chrono::seconds(5))
                throw std::runtime_error("showcase drawable unavailable");
            gui.suspend();
            glfwWaitEventsTimeout(.05);
            continue;
        }
        unavailable.reset();
        const bool autoOpen=tour && !active && !returned && launched<visits.size();
        if (autoOpen) selected=visits[launched];
        bool open=false;
        const bool focused=glfwGetWindowAttrib(window.nativeHandle(),GLFW_FOCUSED);
        const auto begin=Clock::now();
        const auto input=gui.buildPanel(tools,focused,[&](Commands&) {
            open=panel(selected,active,status,preview);
        },false,false,false);
        gui.finishPanelOnly();
        const auto buildUs=micros(begin);
        if (finite && !preview.error.empty()) throw std::runtime_error(preview.error);
        if (!finite && focused && input.keys[Escape]) break;
        window.clear(.047f,.065f,.077f);
        Stats stats;
        const auto submit=Clock::now();
        gui.submit(stats);
        const auto submitUs=micros(submit);
        ++frames;
        if (metrics.is_open()) {
            metrics<<frames<<','<<logical.width<<','<<logical.height<<','<<pixels.width<<','<<pixels.height<<','
                   <<exhibits()[std::size_t(selected)].key<<','<<bool(active)<<','<<buildUs<<','<<submitUs
                   <<','<<stats.guiVertices<<','<<stats.guiIndices<<'\n';
        }
        const bool done=tour?returned && reaped==visits.size():options.frames && frames>=*options.frames;
        if (done && !options.capture.empty()) capture(options.capture,pixels);
        window.swapBuffers();
        if (done) break;
        if (autoOpen || (!finite && open && focused && !active)) {
            const auto& entry=exhibits()[std::size_t(selected)];
            try {
                fs::path stem;
                if (!output.empty()) stem=output/(std::to_string(launched+1)+"-"+entry.key);
                const auto demo=selected==ExhibitId::Replay?assetPath("showcase/timeline.replay"):fs::path{};
                const auto command=exhibitCommand(selected,actualLab,demo,tour?options.childFrames:0,stem);
                const auto pid=process.start(command);
                active=selected;
                ++launched;
                started=Clock::now();
                status.clear();
                std::cout<<"showcase_launch exhibit="<<entry.key<<" pid="<<pid<<std::endl;
            } catch (const std::exception& failure) {
                status=failure.what();
                std::cerr<<"showcase_launch_error exhibit="<<entry.key<<" detail="<<status<<std::endl;
                if (tour) throw;
            }
        }
        if (tour && active) glfwWaitEventsTimeout(.015);
    }
    if (finite && ((tour && reaped!=visits.size()) || (options.frames && frames!=*options.frames)))
        throw std::runtime_error("showcase closed before completion");
    if (active) {
        process.close();
        ++reaped;
        std::cout<<"showcase_closed_child exhibit="<<exhibits()[std::size_t(*active)].key<<std::endl;
    }
    if (metrics.is_open()) metrics.close();
    std::cout<<"status=ok scene=showcase launched="<<launched<<" reaped="<<reaped<<" frames="<<frames<<'\n';
}
}
