#include "tiny2d/TileMap.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace tiny2d {

std::size_t TileMap::checkedCapacity(int width, int height, float tileSize) {
    if (width < 1 || width > maxWidth || height < 1 || height > maxHeight)
        throw std::invalid_argument("tile map dimensions outside 1..128 x 1..96");
    // Both factors have been bounded before multiplication.
    static_assert(std::size_t(maxWidth) * maxHeight <= maxTiles, "tile budget exceeded");
    if (!std::isnormal(tileSize) || tileSize <= 0 ||
        double(tileSize) * std::max(width, height) > std::numeric_limits<float>::max())
        throw std::invalid_argument("invalid tile size or unrepresentable world extent");
    return std::size_t(width) * height;
}

TileMap::TileMap(int width, int height, float tileSize, std::uint32_t seed,
                 std::vector<Tile> cells, TileCoord spawn, TileCoord exit)
    : width_(width), height_(height), tileSize_(tileSize), seed_(seed),
      cells_(std::move(cells)), spawn_(spawn), exit_(exit) {
    if (cells_.size() != checkedCapacity(width_, height_, tileSize_))
        throw std::invalid_argument("tile storage does not match dimensions");
    for (Tile cell : cells_) {
        if (cell != Tile::Floor && cell != Tile::Wall)
            throw std::invalid_argument("unknown tile value");
        if (cell == Tile::Floor) ++walkableCount_;
    }
    if (spawn_ == exit_ || !isWalkable(spawn_.x, spawn_.y) || !isWalkable(exit_.x, exit_.y))
        throw std::invalid_argument("spawn and exit must occupy distinct floor cells");
}

bool TileMap::inBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < width_ && y < height_ && !cells_.empty();
}

Tile TileMap::tile(int x, int y) const {
    if (!inBounds(x, y)) throw std::out_of_range("tile coordinates outside map");
    return cells_[std::size_t(y) * width_ + x];
}

bool TileMap::isWalkable(int x, int y) const {
    return inBounds(x, y) && cells_[std::size_t(y) * width_ + x] == Tile::Floor;
}

TileCoord TileMap::worldToTile(Vector2 position) const {
    auto coordinate = [this](float world) {
        if (!std::isfinite(world)) throw std::invalid_argument("non-finite world coordinate");
        const double cell = std::floor(double(world) / tileSize_);
        if (cell < std::numeric_limits<int>::min() || cell > std::numeric_limits<int>::max())
            throw std::out_of_range("world coordinate exceeds integer tile range");
        return static_cast<int>(cell);
    };
    return {coordinate(position.x), coordinate(position.y)};
}

Vector2 TileMap::tileToWorldCenter(int x, int y) const {
    if (!inBounds(x, y)) throw std::out_of_range("tile center outside map");
    return {static_cast<float>((x + 0.5) * tileSize_),
            static_cast<float>((y + 0.5) * tileSize_)};
}

} // namespace tiny2d
