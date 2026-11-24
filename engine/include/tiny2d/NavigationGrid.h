#pragma once

#include "tiny2d/TileMap.h"

namespace tiny2d {

// One byte per tile: center clearance and four directed, collision-tested edges.
// The grid owns its data and can outlive the TileMap it was built from.
class NavigationGrid {
public:
    NavigationGrid() = default;
    NavigationGrid(const TileMap& map, Vector2 halfSize);

    bool enterable(TileCoord tile) const;
    bool hasEdge(TileCoord from, std::size_t direction) const;
    int width() const { return width_; }
    int height() const { return height_; }
    Vector2 halfSize() const { return halfSize_; }
    float tileSize() const { return tileSize_; }
    std::size_t capacity() const { return nodes_.size(); }

private:
    bool contains(TileCoord tile) const;
    int width_ = 0, height_ = 0;
    float tileSize_ = 0;
    Vector2 halfSize_{};
    std::vector<std::uint8_t> nodes_;
};

} // namespace tiny2d
