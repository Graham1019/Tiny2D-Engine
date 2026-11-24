#include "M5Test.h"
#include "M92MapFixture.h"
#include "tiny2d/GridNavigator.h"
#include "tiny2d/MapGenerator.h"
#include "tiny2d/TileCollision.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <limits>

using namespace tiny2d;
namespace {
const Vector2 body{5, 5};

void knownMatrix() {
    const auto map = mapFixture({"#########", "#...#..##", "#.#.#..##", "#.#.#####",
                                 "#...#...#", "###.###.#", "#########"}, {1,1}, {3,5});
    const std::array<std::string_view,7> expected{
        "---------", "-012-----", "-1-3-----", "-2-4-----", "-345-----", "---6-----", "---------"};
    GridNavigator nav;
    nav.refresh(map, 1, body, map.spawn());
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x) {
            const char c = expected[y][x];
            check(nav.field().distance({x,y}) == (c == '-' ? -1 : c-'0'), "hand distance matrix");
        }
    check(nav.next({3,4}) == TileCoord{2,4}, "west precedes north on equal distances");
    check(!nav.next(map.spawn()) && !nav.next({7,4}), "target and disconnected floors have no successor");
    check(nav.grid().enterable({7,4}) && !nav.grid().enterable({4,4}), "unreachable distinct from solid");
    for (TileCoord invalid : {TileCoord{0,0}, {-1,2}, {std::numeric_limits<int>::max(),0}}) {
        nav.refresh(map, 1, body, invalid);
        check(!nav.next(map.exit()) && nav.stats().visitedNodes == 0 && nav.stats().expandedNodes == 0,
              "illegal targets leave empty fields");
        for (int y = 0; y < map.height(); ++y)
            for (int x = 0; x < map.width(); ++x)
                check(nav.field().distance({x,y}) == -1, "invalid target clears entire old field");
    }
}

// Separate FIFO, neighbor order and edge oracle. The swept volume of an axial
// translation is exactly the bounding rectangle of its two endpoint AABBs.
std::vector<int> reference(const TileMap& map, Vector2 half, TileCoord target) {
    std::vector<int> result(map.capacity(), -1);
    if (!map.inBounds(target.x,target.y) ||
        !canOccupyTiles(map,map.tileToWorldCenter(target.x,target.y),half)) return result;
    std::deque<TileCoord> pending{target};
    result[target.y*map.width()+target.x] = 0;
    while (!pending.empty()) {
        const auto here = pending.front();
        pending.pop_front();
        const auto center = map.tileToWorldCenter(here.x,here.y);
        for (TileCoord neighbor : {TileCoord{here.x,here.y-1}, {here.x+1,here.y},
                                   {here.x,here.y+1}, {here.x-1,here.y}}) {
            if (!map.inBounds(neighbor.x,neighbor.y)) continue;
            int& depth = result[neighbor.y*map.width()+neighbor.x];
            if (depth >= 0) continue;
            const auto other = map.tileToWorldCenter(neighbor.x,neighbor.y);
            const Vector2 mid{(center.x+other.x)/2,(center.y+other.y)/2};
            const Vector2 extents{half.x+std::abs(center.x-other.x)/2,
                                  half.y+std::abs(center.y-other.y)/2};
            if (!canOccupyTiles(map,mid,extents)) continue;
            depth = result[here.y*map.width()+here.x]+1;
            pending.push_back(neighbor);
        }
    }
    return result;
}

void compare(const TileMap& map, const GridNavigator& nav) {
    const auto distances = reference(map, nav.grid().halfSize(), nav.field().target());
    std::size_t reachable = 0;
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x) {
            const TileCoord here{x,y};
            const int depth = distances[y*map.width()+x];
            check(nav.field().distance(here) == depth, "independent swept-box shortest distance");
            reachable += depth >= 0;
            const auto next = nav.next(here);
            if (depth < 1) {
                check(!next, "target or unreachable has no next step");
                continue;
            }
            check(next && distances[next->y*map.width()+next->x] == depth-1, "strict shortest descent");
            bool edge = false;
            for (std::size_t i = 0; i < cardinalOffsets.size(); ++i) {
                auto d = cardinalOffsets[i];
                if (TileCoord{x+d.x,y+d.y} == *next) edge = nav.grid().hasEdge(here,i);
            }
            check(edge, "next step is a legal forward cardinal edge");
        }
    check(reachable == nav.stats().visitedNodes && reachable == nav.stats().expandedNodes,
          "every reachable node discovered and expanded exactly once");
}

void clearanceAndCache() {
    const auto map = mapFixture({"###########", "#....#....#", "#....#....#", "#.........#",
                                 "#....#....#", "#....#....#", "###########"}, {2,3}, {8,3});
    GridNavigator nav;
    nav.refresh(map,1,body,map.spawn());
    check(nav.field().distance(map.exit()) == 6, "small agent fits doorway");
    compare(map,nav);
    const auto at = map.tileToWorldCenter(2,3);
    nav.refresh(map,1,body,map.worldToTile({at.x+4,at.y-5}));
    check(nav.stats().graphBuildCount == 1 && nav.stats().fieldRebuildCount == 1 &&
          nav.stats().cacheReuseCount == 1 && nav.stats().graphBuildUs == 0 &&
          nav.stats().fieldBuildUs == 0, "same-tile motion reuses the field");
    nav.refresh(map,1,body,map.exit());
    check(nav.stats().graphBuildCount == 1 && nav.stats().fieldRebuildCount == 2,"new target reuses graph");
    nav.refresh(map,1,{5,9},map.exit());
    check(!nav.grid().enterable({5,3}) && nav.grid().enterable(map.spawn()) &&
          nav.field().distance(map.spawn()) == -1, "wide body disconnects rooms");
    compare(map,nav);
    nav.refresh(map,2,{5,9},map.exit());
    check(nav.stats().graphBuildCount == 3 && nav.stats().fieldRebuildCount == 4,
          "shape and epoch changes invalidate both caches");
    rejects([&]{nav.refresh(map,2,{0,9},map.exit());});
    check(nav.stats().fieldRebuildCount == 4 && nav.grid().halfSize().x == 5, "invalid shape rejected");
    const auto other = mapFixture({"....","...."}, {1,0},{3,1});
    nav.refresh(other,2,body,other.spawn());
    check(nav.stats().graphBuildCount == 4 && nav.stats().fieldRebuildCount == 5 &&
          nav.field().distance(other.exit()) == 3, "changed dimensions invalidate even with equal seed/epoch");
}

void generated() {
    GridNavigator nav;
    std::uint64_t epoch = 0;
    for (auto size : {TileCoord{5,5}, {17,24}, {64,40}, {128,96}}) {
        for (auto seed : {0u,71u,UINT32_MAX}) {
            const auto world = generateMap(seed,{size.x,size.y});
            for (auto shape : {body, Vector2{8,8}, Vector2{9,5}})
                for (auto target : {world.map.spawn(), world.map.exit()}) {
                    nav.refresh(world.map,++epoch,shape,target);
                    compare(world.map,nav);
                }
            if (size.x == 64 && seed == 71)
                check(world.map.walkableCount() == 1290 && world.connectivity.shortestPathLength == 80,
                      "M9.1 seed 71 floor count and shortest path unchanged");
        }
    }
}
}

int main() {
    try {
        knownMatrix();
        clearanceAndCache();
        generated();
        std::cout << "Known matrix, clearance, epochs and 72 independent swept-volume BFS comparisons passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
