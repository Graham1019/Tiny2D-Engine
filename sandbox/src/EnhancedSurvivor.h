#pragma once

#include "Pipeline.h"
#include "SurvivorPresentation.h"

namespace tiny2d_ui {

// Lifetime belongs to the Survivor window and its current context.
class EnhancedSurvivor {
public:
    explicit EnhancedSurvivor(bool bloom=true) { display_.bloom=bloom; }
    void bloomKey(bool down,bool focused,bool focusLoss=false);
    bool bloom() const { return display_.bloom; }
    bool resize(tiny2d::Size pixels) { return pipeline_.resize(pixels); }
    std::uint64_t storage() const { return pipeline_.storage(); }
    const lab::Stats& stats() const { return stats_; }
    SurvivorRenderStats draw(tiny2d::SceneRenderer&,const tiny2d::Scene&,const tiny2d_game::GameSession&,
                             const tiny2d_vfx::CombatVFX&,const GameHUD&,
                             const tiny2d::Camera2D& world,const tiny2d::Camera2D& ui);
    void menu(tiny2d::SceneRenderer&,const tiny2d::Scene&,const tiny2d::Camera2D&);
private:
    lab::Pipeline pipeline_;
    lab::Display display_;
    lab::Stats stats_;
    bool bloomArmed_=true;
};
}
