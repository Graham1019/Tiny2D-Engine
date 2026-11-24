#pragma once

#include "tiny2d/NavigationGrid.h"

#include <optional>

namespace tiny2d {

class DistanceField {
public:
    // Reuses bounded row-major distance storage and a FIFO on target changes.
    // An invalid target publishes an all-unreachable field.
    void rebuild(const NavigationGrid& grid, TileCoord target);
    TileCoord target() const { return target_; }
    int distance(TileCoord tile) const;
    std::size_t visited() const { return queue_.size(); }
    std::size_t expanded() const { return expanded_; }
    std::optional<TileCoord> next(const NavigationGrid& grid, TileCoord from) const;

private:
    TileCoord target_{-1, -1};
    int width_ = 0, height_ = 0;
    std::vector<int> distances_;
    std::vector<TileCoord> queue_;
    std::size_t expanded_ = 0;
};

} // namespace tiny2d
