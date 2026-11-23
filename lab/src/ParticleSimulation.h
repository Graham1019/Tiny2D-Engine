#pragma once
#include "tiny2d/Vector2.h"
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace lab {
struct ParticleRGB { float r=1, g=1, b=1; };
struct ParticleEmitter {
    bool enabled=true;
    tiny2d::Vector2 position{};
    unsigned rate=720, lifetime=210;  // particles/second, nominal ticks
    float speed=98, direction=0, spread=180, size=5, emission=3.5f;
    ParticleRGB color{.65f,.85f,1.f};
};
enum class FieldKind { Attractor, Repulsor, Vortex, Wind };
struct ForceField2D {
    FieldKind kind=FieldKind::Attractor;
    bool enabled=false;
    tiny2d::Vector2 position{};
    float radius=100, strength=240;
    bool clockwise=false;
    tiny2d::Vector2 direction{1,0}; // Wind only; zero is calm.
};
using ParticleFields = std::array<ForceField2D,8>;
struct ParticleTickInput {
    std::optional<ParticleEmitter> emitter;
    std::optional<ParticleFields> fields;
    unsigned burst=0;
};
struct Particle2D {
    tiny2d::Vector2 position{}, velocity{};
    unsigned age=0, lifetime=1;
    float size=1, emission=1;
    ParticleRGB color{};
    std::uint64_t generation=0;
    bool active=false;
};
struct ParticleHandle {
    std::uint64_t epoch;
    unsigned slot;
    std::uint64_t generation;
};
// Direct GPU instance layout: two groups of four linear floats. Size is diameter.
struct ParticleInstance { float x,y,size,ageFraction,r,g,b,emission; };
struct ParticleRenderSnapshot {
    explicit ParticleRenderSnapshot(unsigned capacity=8192);
    std::vector<ParticleInstance> instances;
};
class ParticleSimulation {
public:
    static constexpr unsigned defaultCapacity=4096, maxCapacity=8192, maxRate=60000, maxBurst=512;
    static constexpr float dt=1.f/60.f, damping=.995f, maxSpeed=350.f, maxAcceleration=1500.f;
    static constexpr float halfWidth=320, halfHeight=160;
    explicit ParticleSimulation(std::uint32_t seed=71, unsigned capacity=defaultCapacity,
                                unsigned rate=ParticleEmitter{}.rate,
                                std::optional<unsigned> steady=std::nullopt);
    ParticleSimulation(const ParticleSimulation&)=delete;
    ParticleSimulation& operator=(const ParticleSimulation&)=delete;
    void reset();
    // Advances exactly one tick. Returns CPU microseconds spent evaluating force fields.
    double step(const ParticleTickInput& input={});
    void snapshot(ParticleRenderSnapshot&) const;
    std::uint64_t tick() const { return tick_; }
    std::uint64_t epoch() const { return epoch_; }
    unsigned capacity() const { return static_cast<unsigned>(slots_.size()); }
    unsigned active() const { return active_; }
    unsigned activeFields() const;
    std::uint64_t dropped() const { return dropped_; }
    const ParticleEmitter& emitter() const { return emitter_; }
    const ParticleFields& fields() const { return fields_; }
    const std::vector<Particle2D>& slots() const { return slots_; }
    ParticleHandle handle(unsigned slot) const;
    const Particle2D* resolve(ParticleHandle) const;
private:
    friend struct ParticleSimulationProbe;
    void spawn();
    float unit();
    std::uint32_t seed_,rng_;
    unsigned initialRate_;
    std::optional<unsigned> steady_;
    ParticleEmitter emitter_;
    ParticleFields fields_;
    std::vector<Particle2D> slots_;
    std::vector<unsigned> free_;
    std::vector<tiny2d::Vector2> accelerations_;
    std::uint64_t epoch_=0,tick_=0,dropped_=0;
    unsigned active_=0,remainder_=0;
};
}
