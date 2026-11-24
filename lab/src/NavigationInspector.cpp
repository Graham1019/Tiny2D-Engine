#include "NavigationInspector.h"
#include <algorithm>

namespace lab {
namespace {
using namespace ImGui;
using namespace tiny2d_game;
using U64=unsigned long long;

const char* effect(UpgradeId id) {
    switch (id) {
    case UpgradeId::Fleet: return "Player speed +12";
    case UpgradeId::Slow: return "Enemy speed -6";
    case UpgradeId::Vitality: return "Max HP +1, heal 1";
    case UpgradeId::Guard: return "Guard duration +12 ticks";
    }
    return "";
}

void inspectPath(NavigationView& view,const NavigationSession& session) {
    const auto& nav=session.navigator();
    const auto tile=nav.field().target();
    const auto& stats=nav.stats();
    Text("Player tile (%d, %d)   Field #%llu",tile.x,tile.y,U64(stats.fieldRebuildCount));
    Text("Cache reuses %llu   Graph builds %llu",U64(stats.cacheReuseCount),U64(stats.graphBuildCount));
    Text("Visited %zu   Expanded %zu",stats.visitedNodes,stats.expandedNodes);
    Checkbox("Distance field",&view.field);
    if (view.field) TextDisabled("Near: green   Far: violet   Unreachable: pink");
    Checkbox("Planned path",&view.path);
    SameLine(); Checkbox("Colliders",&view.bounds);
    if (session.enemies().empty()) { TextDisabled("No active enemies"); return; }
    const int last=int(session.enemies().size())-1;
    view.selected=std::clamp(view.selected,0,last);
    SliderInt("Enemy slot",&view.selected,0,last,"%d",ImGuiSliderFlags_AlwaysClamp);
    const auto& enemy=session.enemies()[view.selected];
    const auto cell=session.world().map().worldToTile(enemy.position);
    Text("Epoch %llu / slot %zu    Position %.1f, %.1f",U64(enemy.epoch),enemy.slot,enemy.position.x,enemy.position.y);
    Text("Tile (%d, %d)   Distance %d",cell.x,cell.y,nav.field().distance(cell));
    if (enemy.waypoint) Text("Waypoint (%d, %d)",enemy.waypoint->x,enemy.waypoint->y);
    else TextDisabled("No committed waypoint");
}
}

void navigationPanel(ToolState& tools,MapEditor& edit,NavigationView& view,Commands& cmd,
                     const NavigationSession& session,bool finite,const History& history,
                     const tiny2d::TimestampQueries* queries,const std::function<void()>& replayPanel,bool playback,
                     const std::function<void()>& timelinePanel,bool selectTimeline) {
    using namespace ImGui;
    const auto& world=session.world();
    const auto& upgrades=session.upgrades();
    const auto size=GetIO().DisplaySize;
    const float width=std::min(timelinePanel?404.f:350.f,std::max(1.f,size.x-12.f));
    SetNextWindowPos({std::max(0.f,size.x-width-8),8},ImGuiCond_FirstUseEver);
    SetNextWindowSize({width,std::min(replayPanel?444.f:340.f,std::max(32.f,size.y-16))},ImGuiCond_FirstUseEver);
    if (Begin(replayPanel?"Replay Inspector":"Navigation Inspector",nullptr,finite?ImGuiWindowFlags_NoInputs:0)) {
        if (!replayPanel) Text("Tick %llu   %s%s",U64(world.tick()),phaseName(session.phase()),world.paused()?" / Paused":"");
        BeginDisabled(playback);
        if (Button(world.paused()?"Resume (P)":"Pause (P)")) cmd.edges.set(Pause);
        SameLine(); BeginDisabled(!world.paused() || session.phase()!=NavigationPhase::Playing);
        if (Button("Step (.)")) cmd.edges.set(Step);
        EndDisabled(); SameLine(); if (Button("Reset (R)")) cmd.edges.set(Reset);
        EndDisabled();
        if (BeginTabBar("Navigation panels")) {
            if (replayPanel && BeginTabItem("Replay")) { replayPanel(); EndTabItem(); }
            if (timelinePanel && BeginTabItem("Timeline",nullptr,selectTimeline?ImGuiTabItemFlags_SetSelected:0)) {
                timelinePanel();
                EndTabItem();
            }
            if (BeginTabItem("World")) {
                Text("Seed %u    Epoch %llu",world.seed(),U64(world.epoch()));
                Text("%d x %d tiles   %.0f units/tile",world.map().width(),world.map().height(),world.map().tileSize());
                Text("Enemies %zu / %d requested",session.enemies().size(),session.requestedEnemies());
                if (*session.spawnShortfallReason()) TextWrapped("%s",session.spawnShortfallReason());
                BeginDisabled(playback);
                SetNextItemWidth(-76);
                InputScalar("Seed",ImGuiDataType_U32,&edit.seedInput);
                if (Button("Regenerate")) edit.regenerate=true;
                EndDisabled();
                if (!edit.error.empty()) TextWrapped("%s",edit.error.c_str());
                Checkbox("Buffer previews",&tools.previews);
                EndTabItem();
            }
            if (BeginTabItem("Navigation")) { inspectPath(view,session); EndTabItem(); }
            if (BeginTabItem("Upgrades")) {
                Text("Offer #%llu   Next tick %llu",U64(upgrades.serial()),U64(session.nextOfferTick()));
                if (const auto& last=upgrades.lastChoice())
                    Text("Last choice #%llu   %s",U64(last->serial),upgradeName(last->id));
                Separator();
                for (auto id:upgradePool)
                    Text("%s  Lv%d    %s%s",upgradeName(id),upgrades.level(id),effect(id),upgrades.eligible(id)?"":" (max)");
                Text("Player %.0f/s   Enemy %.0f/s",upgrades.playerSpeed(),upgrades.enemySpeed());
                Text("HP %d / %d   Guard %d ticks",session.hp(),upgrades.maxHP(),upgrades.guardTicks());
                EndTabItem();
            }
            if (BeginTabItem("Timing")) {
                if (!history.cpu.empty()) {
                    const auto& c=history.cpu.back();
                    Text("Graph build %.2f us (%u)",c.nav.graphUs,c.nav.graphEvents);
                    Text("Field rebuild %.2f us (%u)",c.nav.fieldUs,c.nav.fieldEvents);
                    Text("Enemy navigation update %.2f us",c.nav.enemyUpdateUs);
                    Text("Enemy collisions %.2f us",c.nav.enemyCollisionUs);
                    Text("Upgrade update %.2f us",c.nav.upgradeUs);
                    Text("Visible tile prepare %.2f us",c.map.visiblePrepareUs);
                    Text("CPU frame %.2f us",c.total);
                }
                Checkbox("GPU timing",&tools.timing);
                if (queries && tools.timing) {
                    TextWrapped("GPU timestamp: %s (%d bits)",queries->reason.c_str(),queries->bits);
                    if (!history.gpu.empty() && history.gpu.back().us[0]) Text("GPU frame %.2f us",*history.gpu.back().us[0]);
                    else TextDisabled("GPU time N/A");
                }
                EndTabItem();
            }
            EndTabBar();
        }
    }
    End();
}

std::optional<tiny2d_game::UpgradeChoice> navigationOffer(const NavigationSession& session,bool finite,bool sidePanel) {
    using namespace ImGui;
    const auto& offer=session.upgrades().offer();
    if (!offer) return {};
    const auto size=GetIO().DisplaySize;
    const float width=std::min(360.f,std::max(1.f,size.x-8));
    const float height=GetFrameHeightWithSpacing()*(offer->count+2)+2*GetStyle().WindowPadding.y;
    SetNextWindowSize({width,std::min(height,std::max(32.f,size.y-8))},ImGuiCond_Always);
    if (sidePanel && size.x>=780.f)
        SetNextWindowPos({8.f,std::min(60.f,size.y*.15f)},ImGuiCond_Always);
    else SetNextWindowPos({size.x*.5f,size.y*.5f},ImGuiCond_Always,{.5f,.5f});
    std::optional<UpgradeChoice> result;
    const auto flags=ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|
                     ImGuiWindowFlags_NoSavedSettings|(finite?ImGuiWindowFlags_NoInputs:0);
    if (Begin("Choose an upgrade",nullptr,flags)) {
        if (IsWindowAppearing()) SetWindowFocus();
        Text("Tick %llu    Offer #%llu",U64(session.world().tick()),U64(offer->serial));
        for (int i=0;i<offer->count;++i) {
            const auto id=offer->choices[i];
            if (Button(upgradeName(id),{std::min(92.f,GetContentRegionAvail().x*.38f),0}) && !finite)
                result=UpgradeChoice{offer->epoch,offer->serial,id};
            SameLine(); TextUnformatted(effect(id));
        }
    }
    End();
    return result;
}
}
