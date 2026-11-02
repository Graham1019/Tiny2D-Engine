#pragma once
#include "GameSession.h"
#include <memory>
namespace tiny2d_game {
// CPU ownership boundary. Construct before swap; failure leaves the old session usable.
class SessionController {
public:
    explicit SessionController(CombatConfig config={}, SurvivalRules rules={})
        : config_(config),rules_(rules),game_(std::make_unique<GameSession>(config,rules)) {}
    GameSession& game() { return *game_; }
    const GameSession& game() const { return *game_; }
    bool restartKey(bool pressed) {
        bool edge=pressed&&!previous_; previous_=pressed;
        if(!edge||game_->isPlaying())return false;
        auto replacement=std::make_unique<GameSession>(config_,rules_);
        game_.swap(replacement);
        return true;
    }
private:
    CombatConfig config_;
    SurvivalRules rules_;
    std::unique_ptr<GameSession> game_;
    bool previous_=false;
};
}
