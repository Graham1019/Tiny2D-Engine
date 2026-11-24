#include "tiny2d/NavigationGrid.h"
#include "tiny2d/TileCollision.h"

#include <cmath>
#include <stdexcept>

namespace tiny2d {
namespace {
constexpr std::uint8_t centerClear = 1u << cardinalOffsets.size();
}

NavigationGrid::NavigationGrid(const TileMap& map, Vector2 halfSize)
    : width_(map.width()), height_(map.height()), tileSize_(map.tileSize()),
      halfSize_(halfSize), nodes_(map.capacity()) {
    if (!std::isfinite(halfSize.x) || !std::isfinite(halfSize.y) ||
        halfSize.x <= 0 || halfSize.y <= 0)
        throw std::invalid_argument("navigation requires a positive finite agent half size");

    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            if (canOccupyTiles(map, map.tileToWorldCenter(x, y), halfSize))
                nodes_[y*width_ + x] = centerClear;
        }
    }
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            if (!enterable({x, y})) continue;
            const auto start = map.tileToWorldCenter(x, y);
            for (std::size_t i = 0; i < cardinalOffsets.size(); ++i) {
                const auto d = cardinalOffsets[i];
                if (!enterable({x+d.x, y+d.y})) continue;
                const auto end = map.tileToWorldCenter(x+d.x, y+d.y);
                const auto move = moveAgainstTiles(map, start, halfSize, {end.x-start.x, end.y-start.y});
                if (move.position.x == end.x && move.position.y == end.y)
                    nodes_[y*width_ + x] |= static_cast<std::uint8_t>(1u << i);
            }
        }
    }
}

bool NavigationGrid::contains(TileCoord tile) const {
    return tile.x >= 0 && tile.x < width_ && tile.y >= 0 && tile.y < height_;
}

bool NavigationGrid::enterable(TileCoord tile) const {
    return contains(tile) && (nodes_[tile.y*width_ + tile.x] & centerClear);
}

bool NavigationGrid::hasEdge(TileCoord from, std::size_t direction) const {
    return direction < cardinalOffsets.size() && contains(from) &&
           (nodes_[from.y*width_ + from.x] & (1u << direction));
}

} // namespace tiny2d
