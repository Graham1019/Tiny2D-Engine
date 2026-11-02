#pragma once
#include "tiny2d_game/GameSession.h"
#include "tiny2d_game/Difficulty.h"
#include <memory>
#include <string>
namespace tiny2d_ui {
std::string formatTime(uint64_t seconds);
class GameHUD {
public:
    void update(const tiny2d_game::GameSession&,int width,int height,tiny2d_game::DifficultyLevel level=tiny2d_game::DifficultyLevel::Casual);
    const tiny2d::Scene& scene() const { return *scene_; }
    uint64_t revision() const { return revision_; }
private:
    std::unique_ptr<tiny2d::Scene> scene_=std::make_unique<tiny2d::Scene>();
    std::string key_;
    uint64_t revision_=0, sceneToken_=0, lastKills_=0, flashUntil_=0;
};
}
