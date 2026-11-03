#include "tiny2d_game/Difficulty.h"
#include <cmath>
#include <stdexcept>
namespace tiny2d_game {
DifficultyLevel difficultyLevel(unsigned value) {
    if(value<1||value>5)throw std::invalid_argument("difficulty must be 1..5");
    return static_cast<DifficultyLevel>(value);
}
const char* difficultyName(DifficultyLevel level) {
    static const char* names[]={"CASUAL","EASY","NORMAL","HARD","EXTREME"};
    return names[static_cast<unsigned>(difficultyLevel(static_cast<unsigned>(level)))-1];
}
CombatConfig makeDifficultyConfig(DifficultyLevel level) {
    unsigned k=static_cast<unsigned>(difficultyLevel(static_cast<unsigned>(level)))-1;
    CombatConfig c;
    c.enemySpeed=65*(1+.15*k);
    c.initialSpawnInterval=1.5/(1+.20*k);
    c.minimumSpawnInterval=.6/(1+.20*k);
    c.fireInterval=.6/(1+.08*k);
    c.projectileSpeed=420*(1+.05*k);
    c.validate();return c;
}
uint64_t combatTicks(double seconds) { return static_cast<uint64_t>(std::ceil(seconds*60.0-1e-10)); }
}
