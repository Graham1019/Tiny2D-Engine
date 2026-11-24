#pragma once

#include <cstdint>

namespace tiny2d_game {

inline constexpr std::uint64_t navigationRngIncrement = UINT64_C(0x9e3779b97f4a7c15);
inline constexpr std::uint64_t initialSpawnRng(std::uint32_t seed) {
    return UINT64_C(0x535041574e000000) ^ seed;
}
inline constexpr std::uint64_t initialUpgradeRng(std::uint32_t seed) {
    return UINT64_C(0x5550475241444500) ^ seed;
}

// One SplitMix64 word per selection, including a singleton. The unsigned
// multiply-high mapping duplicates the map generator's portable integer rule.
// The caller owns the stream state. Bounds are positive and at most 12288.
inline unsigned drawNavigationIndex(std::uint64_t& state, unsigned bound) {
    std::uint64_t z = (state += navigationRngIncrement);
    z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
    const auto word = (z ^ (z >> 31)) >> 32;
    return static_cast<unsigned>((word * bound) >> 32);
}

} // namespace tiny2d_game
