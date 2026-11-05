#pragma once
#include "RenderPass.h"
#include "ui/GameHUD.h"
#include "vfx/CombatVFX.h"
namespace tiny2d_ui {
struct SurvivorRenderStats { tiny2d::RenderStats world,ui; };
inline SurvivorRenderStats renderSurvivor(tiny2d::SceneRenderer& renderer,
    const tiny2d::Scene& grid,const tiny2d_game::GameSession& game,
    tiny2d_vfx::CombatVFX& vfx,GameHUD& hud,tiny2d::Camera2D& world,
    tiny2d::Camera2D& ui,int width,int height,tiny2d_game::DifficultyLevel level) {
    // All potentially throwing CPU preparation precedes either GPU pass.
    vfx.present(game,width,height);
    hud.update(game,width,height,level);
    world.setViewportSize(width,height);
    ui.setViewportSize(width,height);ui.setPosition(width/2.f,height/2.f);
    SurvivorRenderStats stats;
    { RenderPass pass(renderer,world);
      renderer.submit(grid);renderer.submit(game.scene());renderer.submit(vfx.scene());pass.end(); }
    stats.world=renderer.stats();
    { RenderPass pass(renderer,ui);
      renderer.submit(hud.scene());renderer.submit(vfx.overlay());pass.end(); }
    stats.ui=renderer.stats();
    return stats; // No complete-frame statistics are published on failure.
}
}
