#pragma once
#include "NavigationInspector.h"
#include "ReplayApp.h"

namespace lab {
// Navigation-specific control boundaries, render preparation and observations
// for the shared tile scene frame loop. All simulation state lives in session.
class NavigationApp {
public:
    NavigationApp(const Options&,const tiny2d::MapParameters&);
    tiny2d_game::NavigationSession live;
    std::optional<ReplayApp> replay;
    const tiny2d_game::NavigationSession& displayed() const;
    bool dispatch(const tiny2d_game::ReplayCommand&);
    NavigationView view;
    bool playing() const;
    void recordInitialBuild(CpuSample&) const;
    void advance(Action,const Options&,Controls&,CpuSample&);
    bool drawOffer(bool finite,bool focused,Controls&,CpuSample&,std::optional<std::uint64_t> panelRevision={},bool sidePanel=false);
    void prepare(Picture&,TileRange,const Options&,CpuSample&) const;
    void printStatus() const;
private:
    bool applyChoice(tiny2d_game::UpgradeChoice,Controls&,CpuSample&,std::optional<std::uint64_t> panelRevision={});
};
}
