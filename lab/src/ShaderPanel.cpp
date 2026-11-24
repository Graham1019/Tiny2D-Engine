#include "ShaderPanel.h"
#include "ShaderReloadManager.h"
#include <algorithm>

namespace lab {
void shaderPanel(ToolState& tools, Commands& commands, GalleryEffect& effect, const ShaderReloadManager& shader,
                 std::uint64_t tick, bool paused, const Display& display,bool finite,const std::string& workspaceError) {
    using namespace ImGui;
    const auto viewport=GetIO().DisplaySize;
    const float width=std::min(342.f,std::max(1.f,viewport.x-12.f));
    SetNextWindowPos({std::max(0.f,viewport.x-width-8),8},ImGuiCond_FirstUseEver);
    SetNextWindowSize({width,std::min(490.f,std::max(32.f,viewport.y-16))},ImGuiCond_FirstUseEver);
    if (Begin("Shader Gallery",nullptr,finite ? ImGuiWindowFlags_NoInputs : 0)) {
        if (!workspaceError.empty()) TextWrapped("%s",workspaceError.c_str());
        const auto shown=effect;
        SetNextItemWidth(-76);
        if (BeginCombo("Exhibit",galleryName(effect))) {
            for (const auto item : galleryEffects)
                if (Selectable(galleryName(item),item==effect)) effect=item;
            EndCombo();
        }
        if (effect==shown) {
            const auto& result=shader.status();
            Text("Last valid version  %llu",static_cast<unsigned long long>(result.version));
            const ImVec4 ink=result.success ? ImVec4{.44f,.91f,.75f,1} : ImVec4{1,.67f,.38f,1};
            TextColored(ink,"%s",result.success ? (result.version==1 ? "Initial load succeeded" : "Reload succeeded")
                                               : "Reload failed - previous version active");
            if (!result.success) TextWrapped("%s",result.error.c_str());
            BeginDisabled(finite);
            if (Button("Reload (F5)")) commands.edges.set(ReloadShader);
            EndDisabled();
            if (finite) { SameLine(); TextDisabled("Fixed run"); }
            Spacing();
            TextDisabled("Fragment file");
            TextWrapped("%s",shader.fragmentPath().c_str());
            TextDisabled("Vertex file");
            TextWrapped("%s",shader.vertexPath().c_str());
        }
        Separator();
        Text("Last tick  %llu",static_cast<unsigned long long>(tick));
        BeginDisabled(finite);
        if (Button(paused ? "Resume (P)" : "Pause (P)")) commands.edges.set(Pause);
        SameLine();
        BeginDisabled(!paused);
        if (Button("Step (.)")) commands.edges.set(Step);
        EndDisabled();
        SameLine();
        if (Button("Reset (R)")) commands.edges.set(Reset);
        int view=static_cast<int>(display.view);
        SetNextItemWidth(-76);
        if (Combo("View",&view,"Scene\0Bright\0Blur\0Final\0")) commands.view=static_cast<View>(view);
        bool bloom=display.bloom;
        if (Checkbox("Bloom (B)",&bloom)) commands.edges.set(Bloom);
        SameLine();
        Checkbox("Buffer previews",&tools.previews);
        EndDisabled();
    }
    End();
}
}
