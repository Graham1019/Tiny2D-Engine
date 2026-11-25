#include "EnhancedSurvivor.h"

namespace tiny2d_ui {
void EnhancedSurvivor::bloomKey(bool down,bool focused,bool focusLoss) {
    if (!focused || focusLoss) bloomArmed_=false;
    if (!focused) return;
    if (down && bloomArmed_) display_.bloom=!display_.bloom;
    bloomArmed_=!down;
}

SurvivorRenderStats EnhancedSurvivor::draw(tiny2d::SceneRenderer& renderer,const tiny2d::Scene& grid,
    const tiny2d_game::GameSession& game,const tiny2d_vfx::CombatVFX& vfx,const GameHUD& hud,
    const tiny2d::Camera2D& world,const tiny2d::Camera2D& ui) {
    SurvivorRenderStats result;
    lab::DrawTools tools;
    tools.submitWorld=[&] {
        return result.world=survivorWorld(renderer,world,grid,game,vfx,2.8f,4.f);
    };
    tools.submitOverlay=[&] { return result.ui=survivorOverlay(renderer,ui,hud,vfx); };
    pipeline_.draw({},display_,stats_,tools);
    return result;
}

void EnhancedSurvivor::menu(tiny2d::SceneRenderer& renderer,const tiny2d::Scene& menu,const tiny2d::Camera2D& ui) {
    lab::DrawTools tools;
    tools.submitOverlay=[&] {
        renderer.render(menu,ui);
        return renderer.stats();
    };
    pipeline_.draw({},display_,stats_,tools);
}
}
