#include "M5Test.h"
#include "M92MapFixture.h"
#include "tiny2d/GridNavigator.h"
#include "tiny2d/MapGenerator.h"
#include "tiny2d/TileCollision.h"

#include <cmath>
#include <limits>

using namespace tiny2d;
namespace {
const Vector2 body{5,5};
bool same(Vector2 a, Vector2 b) { return a.x == b.x && a.y == b.y; }

TileMap switchbacks() {
    return mapFixture({"###########", "#.........#", "#########.#", "#.........#",
                       "#.#########", "#.........#", "###########"}, {1,1},{9,5});
}

void traceSwitchbacks() {
    const auto map = switchbacks();
    const auto start = map.tileToWorldCenter(1,1);
    const auto finish = map.tileToWorldCenter(9,5);
    GridNavigator nav;
    WaypointAgent agent{{start.x+2.5f,start.y+1.5f}, std::nullopt, 1};
    nav.refresh(map,1,body,map.exit());
    nav.advance(map,agent,0.5f);
    check(agent.waypoint == map.spawn() && !same(start,agent.position), "off-center realignment is gradual");
    nav.advance(map,agent,40);
    check(same(start,agent.position) && !agent.waypoint, "caps at alignment waypoint even with surplus budget");
    check(nav.field().distance(map.spawn()) == 28,"known switchback path length");
    std::size_t cellsEntered = 0;
    for (int tick = 0; tick < 600 && !same(agent.position,finish); ++tick) {
        const auto prior = agent.position;
        const TileCoord from = map.worldToTile(prior);
        nav.refresh(map,1,body,map.exit());
        nav.advance(map,agent,1.25f);
        check(canOccupyTiles(map,agent.position,body), "continuous body remains clear around corners");
        const auto to = map.worldToTile(agent.position);
        const double travel = std::hypot(prior.x-agent.position.x,prior.y-agent.position.y);
        check(travel > 0 && travel <= 1.25 && (prior.x == agent.position.x || prior.y == agent.position.y),
              "no stall, diagonal corner-cut or overshoot along path");
        if (from != to) {
            ++cellsEntered;
            check(nav.next(from) == to && nav.field().distance(to) == nav.field().distance(from)-1,
                  "each crossed cell follows a legal descending edge");
        }
    }
    check(same(agent.position,finish) && !agent.waypoint && cellsEntered == 28,"exact target reached");
    for (int i = 0; i < 12; ++i) nav.advance(map,agent,3);
    check(same(finish,agent.position) && nav.stats().fieldRebuildCount == 1,
          "arrival at target center is stable and requires no BFS");
}

void retargetAndInvalidate() {
    const auto map = switchbacks();
    GridNavigator nav;
    nav.refresh(map,20,body,map.exit());
    const auto start = map.tileToWorldCenter(1,1);
    WaypointAgent agent{start,std::nullopt,20};
    nav.advance(map,agent,4);
    check(agent.waypoint == TileCoord{2,1} && agent.position.x == start.x+4,"enters first leg");
    nav.refresh(map,20,body,map.spawn());
    nav.advance(map,agent,1000);
    check(same(agent.position,map.tileToWorldCenter(2,1)) && !agent.waypoint,
          "retarget finishes safely at committed center without overshoot");
    nav.advance(map,agent,5);
    check(agent.waypoint == map.spawn() && agent.position.x == start.x+11,
          "first decision after alignment uses the new field");
    nav.refresh(map,20,body,{-1,-1});
    auto before = agent.position;
    nav.advance(map,agent,2);
    check(same(before,agent.position) && !agent.waypoint,"invalid target freezes motion and clears old segment");
    nav.refresh(map,21,body,map.spawn());
    rejects([&] {nav.advance(map,agent,2);});
    check(same(before,agent.position) && !agent.waypoint,"stale agent cannot enter replacement world");
    WaypointAgent illegal{map.tileToWorldCenter(1,2),TileCoord{2,1},21};
    before = illegal.position;
    rejects([&] {nav.advance(map,illegal,1);});
    check(same(illegal.position,before) && illegal.waypoint == TileCoord{2,1},"illegal start rejected atomically");
    WaypointAgent valid{start,std::nullopt,21};
    for (float bad : {-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
        rejects([&] {nav.advance(map,valid,bad);});
    check(same(valid.position,start) && !valid.waypoint,"invalid travel budget rejected atomically");

    const auto split = mapFixture({"...#...","...#..."}, {1,1},{5,1});
    WaypointAgent isolated{split.tileToWorldCenter(5,1), TileCoord{4,1},22};
    before = isolated.position;
    nav.refresh(split,22,body,split.spawn());
    nav.advance(split,isolated,20);
    check(same(before,isolated.position) && !isolated.waypoint,"disconnected enemy drops its obsolete route");
}

void sharedAndDeterministic() {
    for (auto size : {TileCoord{9,9}, {31,17}, {64,40}}) {
        for (auto seed : {0u,71u,UINT32_MAX}) {
            const auto generated = generateMap(seed,{size.x,size.y});
            const auto& map = generated.map;
            GridNavigator first, second;
            std::vector<WaypointAgent> agents;
            for (int y = 0; y < size.y && agents.size() < 256; ++y)
                for (int x = 0; x < size.x && agents.size() < 256; ++x)
                    if (map.isWalkable(x,y)) agents.push_back({map.tileToWorldCenter(x,y),std::nullopt,9});
            auto replay = agents;
            for (int tick = 0; tick < 110; ++tick) {
                const auto target = tick / 25 % 2 == 0 ? map.spawn() : map.exit();
                first.refresh(map,9,body,target);
                second.refresh(map,9,body,target);
                const float stride = 0.75f + 0.25f * (tick % 4);
                for (std::size_t i = 0; i < agents.size(); ++i) {
                    const auto before = agents[i].position;
                    first.advance(map,agents[i],stride);
                    const double travel = std::hypot(agents[i].position.x-before.x,agents[i].position.y-before.y);
                    check(travel <= stride && canOccupyTiles(map,agents[i].position,body),
                          "changing targets preserve travel budget and collision clearance");
                }
                // Reverse order and query the public inspector API. Neither can affect the field or agents.
                for (std::size_t i = agents.size(); i-- > 0;) {
                    second.next(map.worldToTile(replay[i].position));
                    second.advance(map,replay[i],stride);
                    check(same(agents[i].position,replay[i].position) &&
                          agents[i].waypoint == replay[i].waypoint,"identical per-agent per-tick motion");
                }
                check(first.stats().fieldRebuildCount == std::uint64_t(1+tick/25) &&
                      first.stats().fieldRebuildCount == second.stats().fieldRebuildCount &&
                      first.stats().graphBuildCount == 1 &&
                      first.stats().cacheReuseCount == std::uint64_t(tick-tick/25),
                      "one shared BFS per changed target independent of agent count/inspection");
            }
        }
    }
}
}

int main() {
    try {
        traceSwitchbacks();
        retargetAndInvalidate();
        sharedAndDeterministic();
        std::cout << "28-edge switchback arrival; invalidation, off-center/oversized stride, up to 256 agents passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
