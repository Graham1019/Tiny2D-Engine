#pragma once
#include "Neon.h"
#include "tiny2d/SpriteRenderer.h"
#include "tiny2d/Window.h"
#include <array>
#include <vector>

namespace lab {
enum Pass { World, Extract, Horizontal, Vertical, Output, Overlay, PassCount };
struct Stats {
    unsigned passes = 0, fullscreen = 0;
    std::uint64_t draws = 0, sprites = 0, bytes = 0;
    std::array<double,PassCount> cpuUs{};
    bool complete = false;
};
struct Quad { tiny2d::Vector2 position, size; float angle; tiny2d::Color color; };
struct Picture {
    std::vector<Quad> world, hud;
    tiny2d::Camera2D worldCamera{800,450}, hudCamera{800,450};
    void prepare(const Neon&, const Display&, bool paused, tiny2d::Size logical,
                 tiny2d::Size pixels, const Stats& last);
};
}
