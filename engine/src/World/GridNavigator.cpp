#include "tiny2d/GridNavigator.h"
#include "tiny2d/TileCollision.h"

#include <chrono>
#include <cmath>
#include <stdexcept>

namespace tiny2d {
namespace {
using Clock = std::chrono::steady_clock;
using Microseconds = std::chrono::duration<double, std::micro>;

bool same(Vector2 a, Vector2 b) { return a.x == b.x && a.y == b.y; }

bool legalWaypoint(const NavigationGrid& grid, TileCoord current, TileCoord waypoint) {
    if (current == waypoint) return grid.enterable(waypoint);
    for (std::size_t i = 0; i < cardinalOffsets.size(); ++i) {
        const auto d = cardinalOffsets[i];
        if (TileCoord{current.x + d.x, current.y + d.y} == waypoint)
            return grid.hasEdge(current, i);
    }
    return false;
}
}

void GridNavigator::refresh(const TileMap& map, std::uint64_t epoch, Vector2 halfSize,
                            TileCoord target) {
    const bool newGrid = grid_.width() != map.width() || grid_.height() != map.height() ||
        epoch_ != epoch || seed_ != map.seed() || grid_.tileSize() != map.tileSize() ||
        !same(grid_.halfSize(), halfSize);
    if (!newGrid && target == field_.target()) {
        ++stats_.cacheReuseCount;
        stats_.graphBuildUs = stats_.fieldBuildUs = 0;
        return;
    }
    const auto start = Clock::now();
    if (newGrid) {
        grid_ = NavigationGrid(map, halfSize);
        epoch_ = epoch;
        seed_ = map.seed();
        ++stats_.graphBuildCount;
    }
    const auto graphReady = Clock::now();
    field_.rebuild(grid_, target);
    ++stats_.fieldRebuildCount;
    stats_.graphBuildUs = newGrid ? Microseconds(graphReady-start).count() : 0;
    stats_.fieldBuildUs = Microseconds(Clock::now()-graphReady).count();
    stats_.visitedNodes = field_.visited();
    stats_.expandedNodes = field_.expanded();
}

double GridNavigator::advance(const TileMap& map, WaypointAgent& agent, float maxDistance) const {
    if (grid_.capacity() == 0 || agent.epoch != epoch_ ||
        map.seed() != seed_ || map.width() != grid_.width() || map.height() != grid_.height() ||
        map.tileSize() != grid_.tileSize())
        throw std::invalid_argument("agent and navigation graph must belong to this map epoch");
    if (!std::isfinite(maxDistance) || maxDistance < 0)
        throw std::invalid_argument("invalid waypoint movement budget");

    const TileCoord current = map.worldToTile(agent.position);
    auto waypoint = agent.waypoint;
    if (field_.distance(current) < 0) {
        waypoint.reset();
    } else {
        // Finish the committed axial segment if the player changes tile. A
        // mid-edge diagonal replan could otherwise cut across a blocked corner.
        if (!waypoint || !legalWaypoint(grid_, current, *waypoint)) {
            const auto center = map.tileToWorldCenter(current.x, current.y);
            waypoint = same(center, agent.position) ? next(current) : std::optional<TileCoord>(current);
        }
    }

    Vector2 delta{};
    Vector2 destination = agent.position;
    if (waypoint) {
        destination = map.tileToWorldCenter(waypoint->x, waypoint->y);
        const double dx = double(destination.x)-agent.position.x;
        const double dy = double(destination.y)-agent.position.y;
        const double remaining = std::hypot(dx, dy);
        const double scale = remaining > maxDistance ? maxDistance/remaining : 1.;
        delta = {static_cast<float>(dx*scale), static_cast<float>(dy*scale)};
    }
    const auto start = Clock::now();
    const auto move = moveAgainstTiles(map, agent.position, grid_.halfSize(), delta);
    const double collisionUs = Microseconds(Clock::now()-start).count();
    agent.position = move.position;
    agent.waypoint = same(move.position, destination) ? std::nullopt : waypoint;
    return collisionUs;
}

} // namespace tiny2d
