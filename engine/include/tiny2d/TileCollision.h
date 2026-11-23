#pragma once

#include "tiny2d/TileMap.h"

#include <cstddef>

namespace tiny2d {

struct TileMoveResult {
    Vector2 position;
    bool hitX = false;
    bool hitY = false;
    std::size_t tilesQueried = 0;
};

// Positive-area overlap blocks movement; exact face and corner contact is legal.
// The world outside the rectangular grid is solid, including on open-edge maps.
bool canOccupyTiles(const TileMap& map, Vector2 center, Vector2 halfSize);

// Sweeps X first, then Y at the resolved X. hitX/hitY report clipped motion.
// Rejects an illegal starting box, non-positive half size or non-finite input.
TileMoveResult moveAgainstTiles(const TileMap& map, Vector2 center, Vector2 halfSize,
                                Vector2 displacement);

} // namespace tiny2d
