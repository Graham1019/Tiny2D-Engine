#pragma once

#include "tiny2d/MapGenerator.h"
#include "tiny2d/TileCollision.h"

namespace tiny2d {

// MapParameters includes width, height and tileSize. The player position is
// movement.position, exactly as in MapSession, with one authoritative value.
struct MapSnapshot {
    std::uint32_t seed = 0;
    MapParameters parameters;
    std::vector<Tile> tiles;
    TileCoord spawn, exit;
    std::uint64_t fingerprint = 0, epoch = 0, tick = 0;
    bool paused = false;
    TileMoveResult movement{};
};

} // namespace tiny2d
