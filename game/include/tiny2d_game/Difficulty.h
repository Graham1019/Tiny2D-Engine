#pragma once
#include "CombatConfig.h"
#include <cstdint>
namespace tiny2d_game {
enum class DifficultyLevel : unsigned { Casual=1, Easy, Normal, Hard, Extreme };
DifficultyLevel difficultyLevel(unsigned value);
const char* difficultyName(DifficultyLevel);
CombatConfig makeDifficultyConfig(DifficultyLevel);
// Same upward quantization used by all combat deadlines. Input must be validated.
uint64_t combatTicks(double seconds);
}
