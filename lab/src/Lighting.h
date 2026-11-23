#pragma once
#include "Selection.h"
#include "tiny2d/SpriteRenderer.h"
#include <array>

namespace lab {
// World-space, linear RGB. source is a value identity, never a gameplay pointer.
struct Light2D {
    tiny2d::Vector2 position;
    float radius = 1;
    tiny2d::Color color;
    float intensity = 1;
    LabHandle source{};
    static constexpr float defaultHeight = 32.f;
    float height = defaultHeight; // World +Z above the scene plane.
    void validate() const;
};

class LightList {
public:
    static constexpr unsigned capacity = 32;
    // Validate even an over-budget candidate. The first 32 in source order win.
    void add(const Light2D&);
    void clear() { count_ = dropped_ = 0; }
    unsigned size() const { return count_; }
    unsigned dropped() const { return dropped_; }
    const Light2D* begin() const { return data_.data(); }
    const Light2D* end() const { return begin() + count_; }
    const Light2D* find(LabHandle) const;
private:
    std::array<Light2D,capacity> data_;
    unsigned count_ = 0, dropped_ = 0;
};

float attenuation(const Light2D&, tiny2d::Vector2 worldPoint);
float directionalFactor(const Light2D&, tiny2d::Vector2 worldPoint, WorldNormal);
LightList extractLights(const Neon&, const Display&, std::uint64_t epoch);
}
