#include "tiny2d/MapGenerator.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace tiny2d {
namespace {

// SplitMix64, with defined unsigned wraparound. Use the high 32 bits and
// multiply-high range reduction so the mapping also stays fixed across libraries.
class MapRandom {
public:
    explicit MapRandom(std::uint32_t seed) : state_(seed) {}
    int index(int count) {
        std::uint64_t z = (state_ += UINT64_C(0x9e3779b97f4a7c15));
        z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
        const auto word = (z ^ (z >> 31)) >> 32;
        return static_cast<int>((word * static_cast<unsigned>(count)) >> 32);
    }
private:
    std::uint64_t state_;
};

std::size_t offset(TileCoord c, int width) {
    return std::size_t(c.y) * width + c.x;
}

void carveMaze(std::vector<Tile>& cells, int width, int height, TileCoord spawn, MapRandom& random) {
    std::vector<TileCoord> stack;
    cells[offset(spawn, width)] = Tile::Floor;
    stack.push_back(spawn);
    while (!stack.empty()) {
        const TileCoord here = stack.back();
        std::array<TileCoord, 4> candidates;
        int count = 0;
        for (TileCoord d : cardinalOffsets) {
            const TileCoord next{here.x + 2*d.x, here.y + 2*d.y};
            if (next.x > 0 && next.y > 0 && next.x < width-1 && next.y < height-1 &&
                cells[offset(next, width)] == Tile::Wall)
                candidates[count++] = next;
        }
        if (count == 0) {
            stack.pop_back();
            continue;
        }
        const TileCoord next = candidates[random.index(count)];
        const TileCoord bridge{(here.x + next.x)/2, (here.y + next.y)/2};
        cells[offset(bridge, width)] = Tile::Floor;
        cells[offset(next, width)] = Tile::Floor;
        stack.push_back(next);
    }
}

void carveRooms(std::vector<Tile>& cells, const MapParameters& p, MapRandom& random) {
    for (int room = 0; room < p.roomCount; ++room) {
        // Every odd lattice point belongs to the maze, so each rectangle
        // contains an already-connected floor even on the smallest maps.
        const int cx = 1 + 2*random.index((p.width-1)/2);
        const int cy = 1 + 2*random.index((p.height-1)/2);
        const int rx = 1 + random.index(3);
        const int ry = 1 + random.index(3);
        for (int y = std::max(1, cy-ry); y <= std::min(p.height-2, cy+ry); ++y)
            for (int x = std::max(1, cx-rx); x <= std::min(p.width-2, cx+rx); ++x)
                cells[offset({x,y}, p.width)] = Tile::Floor;
    }
}

void addConnections(std::vector<Tile>& cells, const MapParameters& p, MapRandom& random) {
    if (p.extraConnections == 0) return;
    std::vector<std::size_t> candidates;
    for (int y = 1; y < p.height-1; ++y) {
        for (int x = 1; x < p.width-1; ++x) {
            const auto i = offset({x,y}, p.width);
            if (cells[i] != Tile::Wall) continue;
            if ((cells[i-1] == Tile::Floor && cells[i+1] == Tile::Floor) ||
                (cells[i-p.width] == Tile::Floor && cells[i+p.width] == Tile::Floor))
                candidates.push_back(i);
        }
    }
    for (int k = 0; k < p.extraConnections && !candidates.empty(); ++k) {
        const auto i = random.index(static_cast<int>(candidates.size()));
        cells[candidates[i]] = Tile::Floor;
        candidates[i] = candidates.back();
        candidates.pop_back();
    }
}

TileCoord distantCorner(TileCoord spawn, int lastX, int lastY) {
    // All four odd-lattice corners are guaranteed floor. Maximize a lower
    // bound on path length; ties select the lower coordinate.
    return {spawn.x-1 >= lastX-spawn.x ? 1 : lastX,
            spawn.y-1 >= lastY-spawn.y ? 1 : lastY};
}

} // namespace

MapConnectivity validateConnectivity(const TileMap& map) {
    MapConnectivity result;
    const auto spawn = map.spawn();
    const auto exit = map.exit();
    if (!map.isWalkable(spawn.x, spawn.y) || !map.isWalkable(exit.x, exit.y) || spawn == exit)
        return result;

    std::vector<int> distance(map.capacity(), -1);
    std::vector<TileCoord> queue;
    queue.reserve(map.walkableCount());
    queue.push_back(spawn);
    distance[offset(spawn, map.width())] = 0;
    bool closed = true;
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const auto here = queue[head];
        if (here.x == 0 || here.y == 0 || here.x == map.width()-1 || here.y == map.height()-1)
            closed = false;
        for (auto d : cardinalOffsets) {
            const TileCoord next{here.x + d.x, here.y + d.y};
            if (!map.isWalkable(next.x, next.y)) continue;
            auto& depth = distance[offset(next, map.width())];
            if (depth >= 0) continue;
            depth = distance[offset(here, map.width())] + 1;
            queue.push_back(next);
        }
    }
    result.reachableCount = queue.size();
    result.shortestPathLength = distance[offset(exit, map.width())];
    result.connected = closed && result.reachableCount == map.walkableCount() &&
                       result.shortestPathLength > 0;
    return result;
}

GeneratedMap generateMap(std::uint32_t seed, const MapParameters& p) {
    using Clock = std::chrono::steady_clock;
    using Microseconds = std::chrono::duration<double, std::micro>;
    const auto start = Clock::now();
    const auto capacity = TileMap::checkedCapacity(p.width, p.height, p.tileSize);
    if (p.width < MapParameters::minWidth || p.height < MapParameters::minHeight)
        throw std::invalid_argument("generation requires at least 5 x 5 tiles");
    if (p.roomCount < 0 || p.roomCount > 64 || p.extraConnections < 0 || p.extraConnections > 256)
        throw std::invalid_argument("generation parameters outside carving budgets");
    const int lastX = 2*((p.width-1)/2) - 1;
    const int lastY = 2*((p.height-1)/2) - 1;
    const TileCoord spawn{std::min(31, lastX), std::min(19, lastY)};
    std::vector<Tile> cells(capacity, Tile::Wall);
    MapRandom random(seed);
    carveMaze(cells, p.width, p.height, spawn, random);
    carveRooms(cells, p, random);
    addConnections(cells, p, random);
    TileMap map(p.width, p.height, p.tileSize, seed, std::move(cells), spawn,
                distantCorner(spawn, lastX, lastY));
    const auto built = Clock::now();
    const auto connectivity = validateConnectivity(map);
    if (!connectivity.connected) throw std::logic_error("generated tile map failed connectivity validation");
    const auto checked = Clock::now();
    return {std::move(map), connectivity, Microseconds(built-start).count(),
            Microseconds(checked-built).count()};
}

} // namespace tiny2d
