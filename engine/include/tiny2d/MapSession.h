#pragma once

#include "tiny2d/MapGenerator.h"
#include "tiny2d/TileCollision.h"

#include <cstdint>

namespace tiny2d {

// Signed digital axes (-1, 0, +1). Opposing keys cancel before the tick.
struct MapInput {
    int x = 0;
    int y = 0;
};

MapInput mapWalkInput(std::uint64_t currentTick);

// The session owns its complete CPU world. Borrow map/validation data only
// until the next successful reset or regeneration; epoch identifies this lifetime.
class MapSession {
public:
    explicit MapSession(std::uint32_t seed = 71, const MapParameters& parameters = {});

    const TileMap& map() const { return generated_.map; }
    const MapParameters& parameters() const { return parameters_; }
    const MapConnectivity& connectivity() const { return generated_.connectivity; }
    std::uint32_t seed() const { return map().seed(); }
    std::uint64_t epoch() const { return epoch_; }
    std::uint64_t tick() const { return tick_; }
    Vector2 playerPosition() const { return movement_.position; }
    Vector2 playerHalfSize() const { return {5.f, 5.f}; }
    static constexpr float moveSpeed = 120.f; // world units per second
    const TileMoveResult& collision() const { return movement_; }

    bool paused() const { return paused_; }
    void setPaused(bool paused) { paused_ = paused; }
    void fixedUpdate(MapInput input = {});
    void step(MapInput input = {});
    void step(MapInput input, float unitsPerSecond);
    void reset();
    void regenerate(std::uint32_t seed);
    void regenerate(std::uint32_t seed, const MapParameters& parameters);

    // Last-operation wall-clock observations in microseconds. Collision is a
    // subset of fixed update. None of these values participate in simulation.
    double generationUs() const { return generated_.generationUs; }
    double validationUs() const { return generated_.validationUs; }
    double fixedUpdateUs() const { return fixedUpdateUs_; }
    double collisionUs() const { return collisionUs_; }

private:
    MapParameters parameters_;
    GeneratedMap generated_;
    TileMoveResult movement_;
    std::uint64_t epoch_ = 1;
    std::uint64_t tick_ = 0;
    bool paused_ = false;
    double fixedUpdateUs_ = 0;
    double collisionUs_ = 0;
};

} // namespace tiny2d
