#pragma once
#include "tiny2d_game/GameSession.h"
#include <array>
namespace tiny2d_vfx {
class CombatVFX {
public:
    static constexpr std::size_t capacity=128, spriteBudget=capacity*4+49;
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
    struct Effect { tiny2d::Vector2 p,v; double age=0,life=0; bool death=false,trail=false; };
    void add(Effect);
    std::array<Effect,capacity> effects_{};
    std::array<tiny2d::ObjectId,capacity*4+49> sprites_{};
    std::array<tiny2d::ObjectId,4> edges_{};
    tiny2d::Scene world_,ui_;
    uint64_t token_=0,cursor_=0,lastTrailTick_=0,dropped_=0;
    std::size_t next_=0;
    double damage_=0;
};
}
