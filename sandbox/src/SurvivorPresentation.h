#pragma once
#include "RenderPass.h"
#include "ui/GameHUD.h"
#include "vfx/CombatVFX.h"

namespace tiny2d_ui {
struct SurvivorRenderStats { tiny2d::RenderStats world,ui; };

inline void prepareSurvivor(const tiny2d_game::GameSession& game, tiny2d_vfx::CombatVFX& vfx,
                            GameHUD& hud, tiny2d::Camera2D& world, tiny2d::Camera2D& ui,
                            int width, int height, tiny2d_game::DifficultyLevel level) {
    vfx.present(game,width,height);
    hud.update(game,width,height,level);
    world.setViewportSize(width,height);
    ui.setViewportSize(width,height);
    ui.setPosition(width/2.f,height/2.f);
}

inline tiny2d::RenderStats survivorWorld(tiny2d::SceneRenderer& renderer,const tiny2d::Camera2D& camera,
    const tiny2d::Scene& grid,const tiny2d_game::GameSession& game,const tiny2d_vfx::CombatVFX& vfx,
    float actorGain=1.f,float vfxGain=1.f) {
    RenderPass pass(renderer,camera);
    renderer.submit(grid);
    renderer.submit(game.scene(),actorGain);
    renderer.submit(vfx.scene(),vfxGain);
    pass.end();
    return renderer.stats();
}

inline tiny2d::RenderStats survivorOverlay(tiny2d::SceneRenderer& renderer,const tiny2d::Camera2D& camera,
        const GameHUD& hud,const tiny2d_vfx::CombatVFX& vfx) {
    RenderPass pass(renderer,camera);
    renderer.submit(hud.scene());
    renderer.submit(vfx.overlay());
    pass.end();
    return renderer.stats();
}

inline SurvivorRenderStats renderSurvivor(tiny2d::SceneRenderer& renderer,
    const tiny2d::Scene& grid,const tiny2d_game::GameSession& game,
    tiny2d_vfx::CombatVFX& vfx,GameHUD& hud,tiny2d::Camera2D& world,
    tiny2d::Camera2D& ui,int width,int height,tiny2d_game::DifficultyLevel level) {
    prepareSurvivor(game,vfx,hud,world,ui,width,height,level);
    return {survivorWorld(renderer,world,grid,game,vfx),survivorOverlay(renderer,ui,hud,vfx)};
}
}
