#pragma once

#include "tiny2d/DistanceField.h"
#include "tiny2d/MapSession.h"

namespace tiny2d_game {

// Row-major pool before the production shuffle, evaluated at the epoch's spawn.
struct SpawnSites {
    int minimumDistance;
    std::vector<tiny2d::TileCoord> cells;
};

SpawnSites navigationSpawnSites(const tiny2d::MapSession& world, const tiny2d::DistanceField& spawnField);

} // namespace tiny2d_game
