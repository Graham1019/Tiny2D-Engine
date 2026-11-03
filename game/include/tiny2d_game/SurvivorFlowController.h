#pragma once
#include "Difficulty.h"
#include "SessionController.h"
#include <array>
namespace tiny2d_game {
enum class SurvivorPhase { DifficultySelection, ActiveGame };
struct MenuInput { std::array<bool,5> levels{}; bool enter=false, restart=false, focused=true; };
class SurvivorFlowController {
public:
    explicit SurvivorFlowController(DifficultyLevel level=DifficultyLevel::Normal):selected_(difficultyLevel(static_cast<unsigned>(level))) {}
    SurvivorPhase phase() const { return session_?SurvivorPhase::ActiveGame:SurvivorPhase::DifficultySelection; }
    DifficultyLevel difficulty() const { return selected_; }
    GameSession& game() { if(!session_)throw std::logic_error("no active game");return session_->game(); }
    const GameSession& game() const { if(!session_)throw std::logic_error("no active game");return session_->game(); }
    void start() { if(session_)return;auto next=std::make_unique<SessionController>(makeDifficultyConfig(selected_));session_.swap(next); }
    bool input(const MenuInput& in) {
        // After focus loss require release of every action key before accepting edges.
        if(!in.focused){armed_=false;previous_=in;return false;}
        if(!armed_){previous_=in;bool down=in.enter||in.restart;for(bool v:in.levels)down|=v;armed_=!down;if(armed_&&session_)session_->restartKey(false);return false;}
        auto old=previous_;previous_=in;
        if(session_)return session_->restartKey(in.restart);
        for(unsigned i=0;i<5;++i)if(in.levels[i]&&!old.levels[i])selected_=difficultyLevel(i+1);
        if(in.enter&&!old.enter){start();session_->restartKey(in.restart);return true;}
        return false;
    }
    void fixedUpdate(const GameplayInput& in={}) { if(session_)session_->game().fixedUpdate(in); }
private:
    DifficultyLevel selected_;
    std::unique_ptr<SessionController> session_;
    MenuInput previous_;
    bool armed_=true;
};
}
