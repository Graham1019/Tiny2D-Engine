#pragma once
#include "tiny2d/FixedTimestep.h"
#include "tiny2d/Vector2.h"
#include <array>
#include <bitset>
#include <cstdint>
#include <optional>
#include <string>

namespace lab {
enum class View { Scene, Bright, Blur, Final };
const char* name(View);
struct Display {
    View view = View::Final;
    int strength = 12; // tenths, 0..30
    bool bloom = true, hud = true;
    void validate() const;
};
struct Options {
    std::uint32_t seed = 71;
    std::optional<std::uint32_t> frames;
    int width = 800, height = 450;
    Display display;
    bool inspector = true, timing = true, previews = false;
    std::string capture, metrics;
};
Options parse(int argc, const char* const* argv);

enum Key { Left, Right, Up, Down, Fire, Bloom, Scene, Bright, Blur, Final,
           Weaker, Stronger, Pause, Step, Reset, Hud, Escape, KeyCount };
using Keys = std::bitset<KeyCount>;
struct Motion { int x = 0, y = 0; bool fire = false; };
struct Action { unsigned steps = 0; Motion motion; bool reset = false, exit = false; };
struct Commands {
    Keys edges;
    std::optional<int> strength;
    std::optional<View> view;
};
class Controls {
public:
    bool paused = false;
    Action poll(Keys keys, bool active, double elapsed, Display& display, bool captured=false, Commands ui={},
                bool freshFocus=false);
private:
    Keys previous_, blocked_;
    bool suspended_ = false;
    tiny2d::FixedTimestep clock_;
};

struct Bolt { tiny2d::Vector2 start, end; int age = -1; std::uint64_t generation = 0; };
struct Burst { tiny2d::Vector2 center; int age = -1; float angle = 0; std::uint64_t generation = 0; };
struct Neon {
    explicit Neon(std::uint32_t seed);
    void update(Motion);
    std::uint64_t tick = 0;
    tiny2d::Vector2 player{-62,12};
    std::array<Bolt,12> bolts{};
    std::array<Burst,8> bursts{};
    static constexpr int flight = 44, flash = 27;
    static const std::array<tiny2d::Vector2,4> relays;
private:
    friend struct NeonProbe;
    std::uint32_t rng_;
    unsigned nextBolt_ = 0, nextBurst_ = 0, nextRelay_ = 0;
    void fire();
    void burst(tiny2d::Vector2);
};
}
