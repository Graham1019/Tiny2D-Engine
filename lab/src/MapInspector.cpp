#include "MapInspector.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace lab {
namespace {
using namespace ImGui;
using U64=unsigned long long;

void mapTab(MapEditor& edit,const tiny2d::MapSession& session) {
    const auto& map=session.map();
    const auto& graph=session.connectivity();
    Text("Seed %u    Epoch %llu",map.seed(),U64(session.epoch()));
    Text("%d x %d tiles    Tile size %.1f",map.width(),map.height(),map.tileSize());
    Text("Floor %zu    Wall %zu",size_t(map.walkableCount()),map.capacity()-map.walkableCount());
    Text("Spawn (%d, %d)    Exit (%d, %d)",map.spawn().x,map.spawn().y,map.exit().x,map.exit().y);
    TextColored({.45f,.9f,.76f,1},"%s    %zu reachable",graph.connected ? "Connected" : "Disconnected",
                size_t(graph.reachableCount));
    Text("Shortest path %d steps",graph.shortestPathLength);
    Separator();
    SetNextItemWidth(-92);
    InputScalar("New seed",ImGuiDataType_U32,&edit.seedInput,nullptr,nullptr,"%u");
    if (Button("Regenerate")) edit.regenerate=true;
    if (!edit.error.empty()) TextWrapped("%s",edit.error.c_str());
}
void collisionTab(MapEditor& edit,const tiny2d::MapSession& session) {
    const auto p=session.playerPosition(),half=session.playerHalfSize();
    const auto& map=session.map();
    const auto tile=map.worldToTile(p);
    Text("Player %.2f, %.2f",p.x,p.y);
    Text("AABB %.1f x %.1f",2*half.x,2*half.y);
    Text("Tile (%d, %d)",tile.x,tile.y);
    const auto& hit=session.collision();
    Text("Last tick   X %s   Y %s",hit.hitX ? "hit" : "free",hit.hitY ? "hit" : "free");
    Text("Sweep tile queries %u",unsigned(hit.tilesQueried));
    Checkbox("Show collider",&edit.bounds);
    Separator();
    if (edit.selected) {
        const auto cell=*edit.selected;
        Text("Selected (%d, %d)   %s",cell.x,cell.y,map.isWalkable(cell.x,cell.y) ? "Floor" : "Wall");
    } else TextWrapped("Click a cell in Scene or Final to inspect it.");
}
void timingTab(ToolState& tools,const History& history,const tiny2d::MapSession& session,
               const tiny2d::TimestampQueries* queries) {
    Text("Generation %.1f us   BFS %.1f us",session.generationUs(),session.validationUs());
    if (!history.cpu.empty()) {
        const auto& frame=history.cpu.back();
        const auto& map=frame.map;
        Text("Fixed update %.2f us   Collision %.2f us",frame.update,map.collisionUs);
        Text("Visible tile preparation %.2f us",map.visiblePrepareUs);
        Text("Total %u   Visible %u   Submitted %u",unsigned(map.totalTiles),unsigned(map.visibleTiles),
             unsigned(frame.stats.submittedTiles));
        Text("Tile draws %u   Vertex upload %llu B",unsigned(frame.stats.tileDrawCalls),U64(frame.stats.tileVertexBytes));
        std::array<double,History::capacity> times;
        size_t n=0;
        for (const auto& sample:history.cpu) times[n++]=sample.total;
        std::sort(times.begin(),times.begin()+n);
        auto percentile=[&](double p) {return times[size_t(std::ceil(n*p))-1];};
        Text("CPU frame   %zu samples (us)",n);
        Text("p50 %.0f   p95 %.0f   p99 %.0f",percentile(.50),percentile(.95),percentile(.99));
    }
    Separator();
    Checkbox("GPU timing",&tools.timing);
    if (queries && tools.timing) {
        TextWrapped("Timestamp: %s (%d bits)",queries->reason.c_str(),queries->bits);
        if (!history.gpu.empty() && history.gpu.back().us[0])
            Text("GPU frame #%llu   %.1f us",U64(history.gpu.back().info.id),*history.gpu.back().us[0]);
        else TextDisabled("GPU frame N/A");
    } else TextDisabled("GPU timing off");
}
}

void mapPanel(ToolState& tools,MapEditor& edit,Commands& commands,const tiny2d::MapSession& session,
              const Display& display,bool finite,const History& history,const tiny2d::TimestampQueries* queries) {
    using namespace ImGui;
    const auto size=GetIO().DisplaySize;
    const float width=std::min(324.f,std::max(1.f,size.x-12));
    SetNextWindowPos({std::max(0.f,size.x-width-8),8},ImGuiCond_FirstUseEver);
    SetNextWindowSize({width,std::min(390.f,std::max(32.f,size.y-16))},ImGuiCond_FirstUseEver);
    if (Begin("Map Inspector",nullptr,finite?ImGuiWindowFlags_NoInputs:0)) {
        Text("Pre-update tick %llu   %s",U64(session.tick()),session.paused() ? "Paused" : "Running");
        if (Button(session.paused() ? "Resume (P)" : "Pause (P)")) commands.edges.set(Pause);
        SameLine(); BeginDisabled(!session.paused());
        if (Button("Step (.)")) commands.edges.set(Step);
        EndDisabled(); SameLine();
        if (Button("Reset (R)")) commands.edges.set(Reset);
        if (BeginTabBar("Map panels")) {
            if (BeginTabItem("Map")) { mapTab(edit,session); EndTabItem(); }
            if (BeginTabItem("Collision")) { collisionTab(edit,session); EndTabItem(); }
            if (BeginTabItem("Timing")) { timingTab(tools,history,session,queries); EndTabItem(); }
            if (BeginTabItem("Display")) {
                int view=int(display.view);
                if (Combo("View",&view,"Scene\0Bright\0Blur\0Final\0")) commands.view=static_cast<View>(view);
                bool bloom=display.bloom,hud=display.hud;
                if (Checkbox("Bloom (B)",&bloom)) commands.edges.set(Bloom);
                if (Checkbox("HUD (F1)",&hud)) commands.edges.set(Hud);
                Checkbox("Buffer previews",&tools.previews);
                EndTabItem();
            }
            EndTabBar();
        }
    }
    End();
}
}
