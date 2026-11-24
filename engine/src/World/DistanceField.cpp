#include "tiny2d/DistanceField.h"

namespace tiny2d {

void DistanceField::rebuild(const NavigationGrid& grid, TileCoord target) {
    distances_.assign(grid.capacity(), -1);
    queue_.clear();
    queue_.reserve(grid.capacity());
    target_ = target;
    width_ = grid.width();
    height_ = grid.height();
    expanded_ = 0;
    if (!grid.enterable(target)) return;

    distances_[target.y*width_ + target.x] = 0;
    queue_.push_back(target);
    while (expanded_ < queue_.size()) {
        const TileCoord here = queue_[expanded_++];
        const int nextDepth = distance(here) + 1;
        for (std::size_t i = 0; i < cardinalOffsets.size(); ++i) {
            const auto d = cardinalOffsets[i];
            const TileCoord predecessor{here.x-d.x, here.y-d.y};
            if (!grid.hasEdge(predecessor, i)) continue;
            int& depth = distances_[predecessor.y*width_ + predecessor.x];
            if (depth != -1) continue;
            depth = nextDepth;
            queue_.push_back(predecessor);
        }
    }
}

int DistanceField::distance(TileCoord tile) const {
    if (tile.x < 0 || tile.x >= width_ || tile.y < 0 || tile.y >= height_) return -1;
    return distances_[tile.y*width_ + tile.x];
}

std::optional<TileCoord> DistanceField::next(const NavigationGrid& grid, TileCoord from) const {
    const int depth = distance(from);
    if (depth <= 0) return std::nullopt;
    for (std::size_t i = 0; i < cardinalOffsets.size(); ++i) {
        if (!grid.hasEdge(from, i)) continue;
        const auto d = cardinalOffsets[i];
        const TileCoord neighbor{from.x+d.x, from.y+d.y};
        if (distance(neighbor) == depth-1) return neighbor;
    }
    return std::nullopt;
}

} // namespace tiny2d
