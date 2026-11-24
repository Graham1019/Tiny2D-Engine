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
