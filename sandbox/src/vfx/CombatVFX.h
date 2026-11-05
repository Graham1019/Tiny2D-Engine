#pragma once
#include "tiny2d_game/GameSession.h"
#include <array>
namespace tiny2d_vfx {
enum class EffectKind { Trail, Hit, Death };
class CombatVFX {
public:
    static constexpr std::size_t capacity=128, spriteBudget=capacity*4+49;
    struct Effect { tiny2d::Vector2 p,v; double age=0,life=0; EffectKind kind=EffectKind::Trail; uint64_t createdSequence=0; };
    const std::array<Effect,capacity>& effects() const { return effects_; }
    CombatVFX();
    void reset();
    void consume(const tiny2d_game::GameSession&);
    void update(double dt);
    void present(const tiny2d_game::GameSession&,int width,int height);
    const tiny2d::Scene& scene() const { return world_; }
    const tiny2d::Scene& overlay() const { return ui_; }
    std::size_t active() const;
    tiny2d::Vector2 shake() const;
    uint64_t dropped() const { return dropped_; }
private:
    friend struct CombatVFXTestAccess;
    void add(Effect);
    std::array<Effect,capacity> effects_{};
    std::array<tiny2d::ObjectId,capacity*4+49> sprites_{};
    std::array<tiny2d::ObjectId,4> edges_{};
    tiny2d::Scene world_,ui_;
    uint64_t token_=0,cursor_=0,lastTrailTick_=0,dropped_=0;
    uint64_t createdSequence_=0;
    double damage_=0;
};
}
