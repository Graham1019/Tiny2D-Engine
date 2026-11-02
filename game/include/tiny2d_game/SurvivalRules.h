#pragma once
#include <cstdint>
#include <stdexcept>
namespace tiny2d_game {
struct SurvivalRules {
    uint64_t durationTicks=5400;
    void validate() const {
        // Bound sessions to one day; cooldown additions remain far below overflow.
        if(!durationTicks || durationTicks>86400ull*60)
            throw std::invalid_argument("survival duration must be 1..5184000 ticks");
    }
};
}
