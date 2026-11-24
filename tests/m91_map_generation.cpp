#include "M5Test.h"
#include "tiny2d/MapGenerator.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <iomanip>
#include <limits>
#include <set>

using namespace tiny2d;
namespace {

void dimensionsAndCoordinates() {
    MapParameters p;
    for (int bad : {0, -1, 4, 129, std::numeric_limits<int>::max()}) {
        p.width = bad;
        rejects([&] { generateMap(71, p); });
    }
    p.width = 64;
    for (int bad : {0, 4, 97, std::numeric_limits<int>::max()}) {
        p.height = bad;
        rejects([&] { generateMap(71, p); });
    }
    p.height = 40;
    const float inf = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float huge = std::numeric_limits<float>::max();
    for (float bad : {0.0f, -1.0f, inf, nan, huge, std::numeric_limits<float>::denorm_min()}) {
        p.tileSize = bad;
        rejects([&] { generateMap(71, p); });
    }
    p.tileSize = 12.5f;
    for (int bad : {-1, 65}) {
        p.roomCount = bad;
        rejects([&] { generateMap(71, p); });
    }
    p.roomCount = 8;
    for (int bad : {-1, 257}) {
        p.extraConnections = bad;
        rejects([&] { generateMap(71, p); });
    }
    p.extraConnections = 12;
    const auto generated = generateMap(71, p);
    const auto& map = generated.map;
    check(map.width() == 64 && map.height() == 40 && map.capacity() == 2560 &&
          map.seed() == 71 && map.tileSize() == 12.5f, "map dimensions and identity");
    for (TileCoord outside : {TileCoord{-1,0}, {0,-1}, {64,39}, {63,40},
                              {std::numeric_limits<int>::min(),std::numeric_limits<int>::max()}}) {
        check(!map.inBounds(outside.x, outside.y) && !map.isWalkable(outside.x, outside.y), "exterior solid");
        rejects([&] { map.tile(outside.x, outside.y); });
        rejects([&] { map.tileToWorldCenter(outside.x, outside.y); });
    }
    check(map.worldToTile({-0.001f,-12.501f}) == TileCoord{-1,-2}, "negative floor");
    check(map.worldToTile({-12.5f,12.5f}) == TileCoord{-1,1}, "exact boundaries");
    check(map.worldToTile({0,0}) == TileCoord{0,0}, "origin");
    check(map.worldToTile({800,500}) == TileCoord{64,40}, "upper exclusive extent");
    for (float bad : {inf, -inf, nan, huge, -huge}) {
        rejects([&] { map.worldToTile({0,bad}); });
        rejects([&] { map.worldToTile({bad,0}); });
    }
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            check(map.worldToTile(map.tileToWorldCenter(x,y)) == TileCoord{x,y}, "world center round trip");
            check(map.tile(x,y) == map.cells()[std::size_t(y)*map.width()+x], "row-major layout");
        }
    }
    const std::vector<Tile> floor(4, Tile::Floor);
    rejects([&] { TileMap(2,3,16,1,floor,{0,0},{1,1}); });
    rejects([&] { TileMap(2,2,16,1,floor,{0,0},{0,0}); });
    rejects([&] { TileMap(2,2,16,1,floor,{0,0},{1,2}); });
    auto invalid = floor;
    invalid[0] = Tile::Wall;
    rejects([&] { TileMap(2,2,16,1,invalid,{0,0},{1,1}); });
    invalid[2] = static_cast<Tile>(7);
    rejects([&] { TileMap(2,2,16,1,invalid,{1,0},{1,1}); });
}

// Separate implementation: traverse from exit, reading the raw grid and
// walking in a different order. Verify distances, every floor and every edge.
void independentBfs(const GeneratedMap& generated) {
    const auto& m = generated.map;
    const auto& cells = m.cells();
    const int w = m.width();
    const int h = m.height();
    const int start = m.exit().y*w + m.exit().x;
    std::deque<int> open{start};
    std::vector<int> distances(cells.size(), -1);
    distances[start] = 0;
    while (!open.empty()) {
        const int n = open.front();
        open.pop_front();
        for (int next : {n-w, n-1, n+w, n+1}) {
            check(next >= 0 && next < w*h, "reached open top/bottom edge");
            if (cells[next] == Tile::Wall || distances[next] >= 0) continue;
            distances[next] = distances[n] + 1;
            open.push_back(next);
        }
    }
    std::size_t floors = 0;
    for (int n = 0; n < w*h; ++n) {
        if (n%w == 0 || n%w == w-1 || n/w == 0 || n/w == h-1)
            check(cells[n] == Tile::Wall, "perimeter wall");
        if (cells[n] == Tile::Floor) {
            ++floors;
            check(distances[n] >= 0, "disconnected floor");
        }
    }
    const int steps = distances[m.spawn().y*w + m.spawn().x];
    check(steps > 0 && steps == generated.connectivity.shortestPathLength, "independent path length");
    check(floors == m.walkableCount() && floors == generated.connectivity.reachableCount &&
          generated.connectivity.connected, "independent floor accounting");
    if (w == 64 && h == 40) check(steps >= 48, "default path length");
}

void invalidConnectivity() {
    std::vector<Tile> cells(25, Tile::Wall);
    cells[6] = cells[7] = cells[8] = cells[18] = Tile::Floor;
    TileMap isolated(5,5,16,1,cells,{1,1},{3,1});
    auto result = validateConnectivity(isolated);
    check(!result.connected && result.reachableCount == 3 && result.shortestPathLength == 2,
          "isolated floor must invalidate a reachable exit");
    TileMap unreachable(5,5,16,1,cells,{1,1},{3,3});
    check(validateConnectivity(unreachable).shortestPathLength == -1, "unreachable exit");
    cells[18] = Tile::Wall;
    cells[5] = Tile::Floor;
    TileMap open(5,5,16,1,cells,{1,1},{3,1});
    result = validateConnectivity(open);
    check(!result.connected && result.reachableCount == open.walkableCount(), "open perimeter");
}

std::uint64_t fingerprint(const TileMap& map) {
    std::uint64_t hash = UINT64_C(14695981039346656037);
    for (Tile tile : map.cells()) {
        hash ^= static_cast<std::uint8_t>(tile);
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

void seedSweep() {
    constexpr std::array<TileCoord, 6> sizes{{{5,5},{6,7},{17,25},{64,40},{96,64},{128,96}}};
    for (auto size : sizes) {
        const MapParameters p{size.x,size.y};
        const auto first = generateMap(0, p);
        std::set<std::uint64_t> unique;
        std::size_t changed = 0;
        for (std::uint32_t i = 0; i < 32; ++i) {
            const std::uint32_t seed = i == 31 ? UINT32_MAX : i*UINT32_C(2654435761);
            const auto a = generateMap(seed, p);
            const auto b = generateMap(seed, p);
            independentBfs(a);
            check(a.map.seed() == seed && a.map.cells() == b.map.cells() &&
                  a.map.spawn() == b.map.spawn() && a.map.exit() == b.map.exit() &&
                  a.connectivity.shortestPathLength == b.connectivity.shortestPathLength,
                  "same seed changed output");
            check(a.generationUs >= 0 && a.validationUs >= 0, "invalid generation timings");
            unique.insert(fingerprint(a.map));
            for (std::size_t j = 0; j < a.map.capacity(); ++j)
                if (a.map.cells()[j] != first.map.cells()[j]) ++changed;
        }
        const double difference = 100.0 * changed / (31.0 * first.map.capacity());
        if (size.x >= 17) check(unique.size() == 32 && difference > 10, "seed variation");
        std::cout << size.x << 'x' << size.y << " seeds=32 distinct=" << unique.size()
                  << " mean cell difference vs seed 0=" << std::fixed << std::setprecision(2)
                  << difference << "%\n";
    }
    independentBfs(generateMap(71, MapParameters{13,19,16,0,0}));
    independentBfs(generateMap(71, MapParameters{13,19,16,64,256}));
}

} // namespace

int main() {
    try {
        dimensionsAndCoordinates();
        invalidConnectivity();
        seedSweep();
        const auto baseline = generateMap(71);
        check(fingerprint(baseline.map) == UINT64_C(0xde25ba54c55fec8b),
              "seed 71 generation sequence changed");
        std::cout << "seed 71 default tile FNV-1a=" << std::hex << fingerprint(baseline.map) << '\n';
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
