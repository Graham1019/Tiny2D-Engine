#pragma once
#include "tiny2d_game/Difficulty.h"
#include "tiny2d/Scene.h"
#include <memory>
#include <string>
namespace tiny2d_ui {
class DifficultyMenu {
public:
    void update(tiny2d_game::DifficultyLevel,int width,int height);
    const tiny2d::Scene& scene() const { return *scene_; }
private:
    std::unique_ptr<tiny2d::Scene> scene_=std::make_unique<tiny2d::Scene>();
    std::string key_;
};
}
