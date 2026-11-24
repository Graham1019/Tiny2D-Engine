#pragma once

#include "tiny2d/TileMap.h"

namespace tiny2d {

struct MapParameters {
    static constexpr int minWidth = 5;
    static constexpr int minHeight = 5;
    static constexpr int maxWidth = TileMap::maxWidth;
    static constexpr int maxHeight = TileMap::maxHeight;

    int width = 64;
    int height = 40;
    float tileSize = 16;
    int roomCount = 8;         // Number of connected rectangles to carve, 0..64.
    int extraConnections = 12; // Maximum additional entrances, 0..256.
};

struct MapConnectivity {
    std::size_t reachableCount = 0;
    int shortestPathLength = -1; // Four-neighbor steps from spawn to exit.
    bool connected = false;      // All floors reachable and perimeter closed.
};

struct GeneratedMap {
    TileMap map;
    MapConnectivity connectivity;
    double generationUs; // Construction only.
    double validationUs; // Independent BFS and perimeter check.
};

// Stateless. A fresh private PRNG is created for each generation request.
GeneratedMap generateMap(std::uint32_t seed, const MapParameters& parameters = {});
MapConnectivity validateConnectivity(const TileMap& map);

} // namespace tiny2d
