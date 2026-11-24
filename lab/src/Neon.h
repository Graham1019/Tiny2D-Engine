#pragma once
#include "GalleryEffect.h"
#include "NormalMaterial.h"
#include "tiny2d/FixedTimestep.h"
#include "tiny2d/Vector2.h"
#include <array>
#include <bitset>
#include <cstdint>
#include <optional>
#include <string>

namespace lab {
enum class View { Scene, Bright, Blur, Final, Light, Normal, Shadow };
const char* name(View);
struct Display {
    View view = View::Final;
    int strength = 12; // tenths, 0..30
    bool bloom = true, hud = true;
    bool lighting = false;
    float ambient = .32f, lightStrength = 1.f, radiusMultiplier = 1.f;
    static constexpr float maxAmbient = 2.f, maxLightStrength = 4.f;
    static constexpr float minRadiusMultiplier = .25f, maxRadiusMultiplier = 3.f;
    bool normalMapping = false;
    NormalMaterial2D normalMaterial;
    static constexpr unsigned maxShadowLights = 8;
    bool shadows = false, demoOccluders = true;
    unsigned shadowLights = 4;
    void validate() const;
};
enum class LabScene { Neon, Particles, Shaders, Map, Navigation, Replay };
struct Options {
    LabScene scene = LabScene::Neon;
    GalleryEffect effect = GalleryEffect::Aurora;
    unsigned particleCapacity = 4096, particleRate = 720;
    std::optional<unsigned> particleSteady;
    bool particleDemo = false;
    int mapWidth = 64, mapHeight = 40;
    bool mapWalk = false;
    unsigned navEnemies = 16;
    bool navWalk = false, navChooseLast = false, navField = false, navPath = false;
    std::uint32_t seed = 71;
    std::optional<std::uint32_t> frames;
    int width = 800, height = 450;
    Display display;
    bool inspector = true, timing = true, previews = false;
    std::string capture, metrics;
    std::string snapshotIn, snapshotOut, recordPath, replayPath, branchIn, branchOut;
    std::optional<std::uint32_t> seekEvent, forkAt;
    std::uint32_t branchId=0;
    bool timelineView=false;
};
Options parse(int argc, const char* const* argv);

enum Key { Left, Right, Up, Down, Fire, Bloom, Scene, Bright, Blur, Final,
           Weaker, Stronger, Pause, Step, Reset, Hud, Escape, ToggleLighting, ShowLight,
           ToggleNormal, ShowNormal, ToggleShadows, ShowShadow, ReloadShader, KeyCount };
using Keys = std::bitset<KeyCount>;
struct Motion { int x = 0, y = 0; bool fire = false; };
struct Action { unsigned steps = 0; Motion motion; bool reset = false, exit = false; Keys pressed; };
struct Commands {
    Keys edges;
    std::optional<int> strength;
    std::optional<View> view;
    std::optional<float> ambient, lightStrength, radiusMultiplier;
    std::optional<float> normalStrength;
    std::optional<NormalMaterialKind> normalMaterial;
    std::optional<unsigned> shadowLights;
};
class Controls {
public:
    bool paused = false;
    void clearElapsed() { clock_.reset(); }
    void replaceInstance(Keys held,bool manualPause) {
        blocked_ |= held;
        paused=manualPause;
        clock_.reset();
    }
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
