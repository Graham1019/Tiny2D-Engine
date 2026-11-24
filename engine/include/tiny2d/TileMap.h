#pragma once

#include "tiny2d/Vector2.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tiny2d {

enum class Tile : std::uint8_t { Wall, Floor };

struct TileCoord {
    int x = 0;
    int y = 0;
};

constexpr bool operator==(TileCoord a, TileCoord b) { return a.x == b.x && a.y == b.y; }
constexpr bool operator!=(TileCoord a, TileCoord b) { return !(a == b); }

inline constexpr std::array<TileCoord, 4> cardinalOffsets{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

// Immutable row-major value. Spawn and exit are distinct floor cells. Custom
// fixtures may have disconnected floors or open edges; validateConnectivity
// checks those stronger requirements when publishing a generated map.
class TileMap {
public:
    static constexpr int maxWidth = 128;
    static constexpr int maxHeight = 96;
    static constexpr std::size_t maxTiles = 12288;

    TileMap(int width, int height, float tileSize, std::uint32_t seed,
            std::vector<Tile> cells, TileCoord spawn, TileCoord exit);

    // Validate dimensions and report the exact number of tiles before allocating.
    static std::size_t checkedCapacity(int width, int height, float tileSize);
    int width() const { return width_; }
    int height() const { return height_; }
    std::size_t capacity() const { return cells_.size(); }
    float tileSize() const { return tileSize_; }
    std::uint32_t seed() const { return seed_; }
    TileCoord spawn() const { return spawn_; }
    TileCoord exit() const { return exit_; }
    std::size_t walkableCount() const { return walkableCount_; }
    const std::vector<Tile>& cells() const { return cells_; }

    bool inBounds(int x, int y) const;
    Tile tile(int x, int y) const;               // Throws out_of_range outside the map.
    bool isWalkable(int x, int y) const;         // Exterior is solid.
    TileCoord worldToTile(Vector2 position) const; // May return out-of-bounds coordinates.
    Vector2 tileToWorldCenter(int x, int y) const;

private:
    int width_, height_;
    float tileSize_;
    std::uint32_t seed_;
    std::vector<Tile> cells_;
    TileCoord spawn_, exit_;
    std::size_t walkableCount_ = 0;
};

} // namespace tiny2d
