#pragma once

#include "tiny2d/DistanceField.h"

namespace tiny2d {

// Committed segment endpoint and position affect the next tick. Both belong
// to the simulation snapshot. Epoch prevents using an old agent on a new map.
struct WaypointAgent {
    Vector2 position;
    std::optional<TileCoord> waypoint;
    std::uint64_t epoch = 0;
};

struct NavigationStats {
    std::uint64_t graphBuildCount = 0, fieldRebuildCount = 0, cacheReuseCount = 0;
    std::size_t visitedNodes = 0, expandedNodes = 0; // most recent field
    double graphBuildUs = 0, fieldBuildUs = 0;       // this refresh only
};

class GridNavigator {
public:
    // One call from the world owner per simulation tick, after player motion.
    // The owner issues a new epoch whenever replacing its immutable map.
    void refresh(const TileMap& map, std::uint64_t epoch, Vector2 halfSize, TileCoord target);
    const NavigationGrid& grid() const { return grid_; }
    const DistanceField& field() const { return field_; }
    const NavigationStats& stats() const { return stats_; }
    std::optional<TileCoord> next(TileCoord from) const { return field_.next(grid_, from); }

    // Complete at most the current segment. Returns measured collision CPU us.
    // A stale agent or illegal initial AABB is rejected without moving it.
    double advance(const TileMap& map, WaypointAgent& agent, float maxDistance) const;

private:
    NavigationGrid grid_;
    DistanceField field_;
    std::uint64_t epoch_ = 0;
    std::uint32_t seed_ = 0;
    NavigationStats stats_;
};

} // namespace tiny2d
