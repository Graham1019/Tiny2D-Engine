#include "tiny2d/MapSession.h"

#include "tiny2d/FixedTimestep.h"

#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace tiny2d {
namespace {

std::uint64_t tileFingerprint(const TileMap& map) {
    std::uint64_t hash = UINT64_C(14695981039346656037);
    for (auto tile : map.cells()) {
        hash ^= static_cast<std::uint8_t>(tile);
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

TileMoveResult spawnPlayer(const GeneratedMap& candidate, Vector2 halfSize) {
    const auto spawn = candidate.map.spawn();
    const Vector2 center = candidate.map.tileToWorldCenter(spawn.x, spawn.y);
    if (!canOccupyTiles(candidate.map, center, halfSize))
        throw std::invalid_argument("generated spawn cannot contain the player AABB");
    return {center};
}

} // namespace

MapInput mapWalkInput(std::uint64_t currentTick) {
    static constexpr std::array<MapInput, 8> directions{{
        {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}
    }};
    return directions[(currentTick/36) % directions.size()];
}

MapSession::MapSession(std::uint32_t seed, const MapParameters& parameters)
    : parameters_(parameters), generated_(generateMap(seed, parameters)),
      movement_(spawnPlayer(generated_, playerHalfSize())) {}

MapSnapshot MapSession::captureSnapshot() const {
    return {seed(), parameters_, map().cells(), map().spawn(), map().exit(), tileFingerprint(map()),
            epoch_, tick_, paused_, movement_};
}

MapSession MapSession::fromSnapshot(const MapSnapshot& s) {
    if (s.epoch == 0) throw std::invalid_argument("world.epoch must be positive");
    if (s.tiles.size() != TileMap::checkedCapacity(s.parameters.width, s.parameters.height,
                                                   s.parameters.tileSize))
        throw std::invalid_argument("world.tiles length differs from dimensions");
    // Generation checks the carving budgets, bounds, closed perimeter, spawn,
    // exit, and connectivity. Equality then proves those properties for s.tiles.
    MapSession candidate(s.seed, s.parameters);
    const auto& map = candidate.map();
    if (s.spawn != map.spawn() || s.exit != map.exit() || s.tiles != map.cells() ||
        s.fingerprint != tileFingerprint(map))
        throw std::invalid_argument("world map differs from the seed, parameters or tile fingerprint");
    const auto& move = s.movement;
    if (!canOccupyTiles(map, move.position, candidate.playerHalfSize()))
        throw std::invalid_argument("world.movement.position is not a legal player AABB");
    if (s.tick == 0) {
        const auto spawn = candidate.playerPosition();
        if (move.position.x != spawn.x || move.position.y != spawn.y || move.hitX || move.hitY ||
            move.tilesQueried != 0)
            throw std::invalid_argument("world.movement at tick zero differs from spawn");
    } else if (move.tilesQueried == 0 || move.tilesQueried > 3 * map.capacity()) {
        // One initial box plus at most one full row-major sweep on each axis.
        throw std::invalid_argument("world.movement.tilesQueried outside collision bounds");
    }
    candidate.movement_ = move;
    candidate.epoch_ = s.epoch;
    candidate.tick_ = s.tick;
    candidate.paused_ = s.paused;
    return candidate;
}

void MapSession::fixedUpdate(MapInput input) {
    if (!paused_) step(input);
}

void MapSession::step(MapInput input) {
    step(input, moveSpeed);
}

void MapSession::step(MapInput input, float unitsPerSecond) {
    using Clock = std::chrono::steady_clock;
    using Microseconds = std::chrono::duration<double, std::micro>;
    const auto start = Clock::now();
    if (input.x < -1 || input.x > 1 || input.y < -1 || input.y > 1)
        throw std::invalid_argument("map input axes must be -1, 0, or 1");
    if (!std::isfinite(unitsPerSecond) || unitsPerSecond < 0.f || unitsPerSecond > 100000.f)
        throw std::invalid_argument("map movement speed must be within 0..100000");
    if (tick_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("map tick exhausted");
    constexpr double diagonalFactor = 0.70710678118654752440;
    const double stride = unitsPerSecond * FixedTimestep::step *
        (input.x != 0 && input.y != 0 ? diagonalFactor : 1.);
    const Vector2 delta{static_cast<float>(input.x*stride), static_cast<float>(input.y*stride)};
    const auto collisionStart = Clock::now();
    movement_ = moveAgainstTiles(map(), playerPosition(), playerHalfSize(), delta);
    const auto collisionEnd = Clock::now();
    ++tick_;
    collisionUs_ = Microseconds(collisionEnd - collisionStart).count();
    fixedUpdateUs_ = Microseconds(Clock::now() - start).count();
}

void MapSession::reset() {
    regenerate(seed(), parameters_);
}

void MapSession::regenerate(std::uint32_t seed) {
    regenerate(seed, parameters_);
}

void MapSession::regenerate(std::uint32_t seed, const MapParameters& parameters) {
    if (epoch_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("map epoch exhausted");
    auto candidate = generateMap(seed, parameters);
    const auto player = spawnPlayer(candidate, playerHalfSize());
    generated_ = std::move(candidate);
    parameters_ = parameters;
    movement_ = player;
    tick_ = 0;
    ++epoch_;
    fixedUpdateUs_ = collisionUs_ = 0.;
}

} // namespace tiny2d
