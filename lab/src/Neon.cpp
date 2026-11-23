#include "Neon.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string_view>

namespace lab {
namespace {
std::uint32_t number(std::string_view value, std::string_view option) {
    std::uint32_t result = 0;
    const auto end = value.data() + value.size();
    const auto parsed = std::from_chars(value.data(),end,result);
    if (parsed.ec != std::errc{} || parsed.ptr != end)
        throw std::invalid_argument(std::string(option) + " expects an unsigned 32-bit decimal integer");
    return result;
}
float displayValue(std::string_view value, std::string_view option, float maximum) {
    double result=0;
    const auto end=value.data()+value.size();
    const auto parsed=std::from_chars(value.data(),end,result);
    if (parsed.ec!=std::errc{} || parsed.ptr!=end || !std::isfinite(result) ||
        std::signbit(result) || result>maximum)
        throw std::invalid_argument(std::string(option) + " is outside its supported range");
    return static_cast<float>(result);
}
}
const char* name(View v) {
    switch (v) {
    case View::Scene: return "scene";
    case View::Bright: return "bright";
    case View::Blur: return "blur";
    case View::Final: return "final";
    case View::Light: return "light";
    case View::Normal: return "normal";
    }
    throw std::invalid_argument("invalid view");
}
void Display::validate() const {
    (void)name(view);
    if (strength < 0 || strength > 30) throw std::invalid_argument("strength outside 0..30 tenths");
    if (!(ambient>=0 && ambient<=maxAmbient && lightStrength>=0 && lightStrength<=maxLightStrength &&
          radiusMultiplier>=minRadiusMultiplier && radiusMultiplier<=maxRadiusMultiplier))
        throw std::invalid_argument("lighting parameters outside supported ranges");
    if (view==View::Light && !lighting) throw std::invalid_argument("Light view requires lighting on");
    normalMaterial.validate();
    if (normalMapping && !lighting) throw std::invalid_argument("normal mapping requires lighting on");
    if (view==View::Normal && !normalMapping) throw std::invalid_argument("Normal view requires normal mapping on");
}
Options parse(int argc, const char* const* argv) {
    Options o;
    std::optional<bool> inspector, timing;
    for (int i=1; i<argc; ++i) {
        std::string_view key = argv[i];
        if (++i == argc) throw std::invalid_argument("missing value for " + std::string(key));
        std::string_view value = argv[i];
        if (key == "--scene" && value == "neon") continue;
        if (key == "--seed") o.seed = number(value,key);
        else if (key == "--frames") {
            o.frames = number(value,key);
            if (!*o.frames) throw std::invalid_argument("--frames must be positive");
        } else if (key == "--view") {
            if (value == "scene") o.display.view = View::Scene;
            else if (value == "bright") o.display.view = View::Bright;
            else if (value == "blur") o.display.view = View::Blur;
            else if (value == "final") o.display.view = View::Final;
            else if (value == "light") o.display.view = View::Light;
            else if (value == "normal") o.display.view = View::Normal;
            else throw std::invalid_argument("unknown --view");
        } else if (key == "--lighting" && (value == "on" || value == "off")) o.display.lighting=value=="on";
        else if (key == "--ambient") o.display.ambient=displayValue(value,key,Display::maxAmbient);
        else if (key == "--normal-mapping" && (value=="on" || value=="off")) o.display.normalMapping=value=="on";
        else if (key == "--normal-strength") o.display.normalMaterial.strength=displayValue(value,key,NormalMaterial2D::maxStrength);
        else if (key == "--normal-material") {
            if (value=="flat") o.display.normalMaterial.kind=NormalMaterialKind::Flat;
            else if (value=="tile") o.display.normalMaterial.kind=NormalMaterialKind::ProceduralTile;
            else throw std::invalid_argument("--normal-material expects flat or tile");
        }
        else if (key == "--bloom" && (value == "on" || value == "off")) o.display.bloom = value == "on";
        else if ((key=="--inspector" || key=="--gpu-timing") && (value=="on" || value=="off")) {
            (key=="--inspector" ? inspector : timing) = value=="on";
        } else if (key=="--previews" && (value=="off" || value=="all")) o.previews=value=="all";
        else if (key=="--metrics" && !value.empty()) o.metrics=value;
        else if (key == "--size") {
            const auto split = value.find('x');
            if (split == value.npos) throw std::invalid_argument("--size expects WIDTHxHEIGHT");
            const auto w = number(value.substr(0,split),key), h = number(value.substr(split+1),key);
            if (!w || !h || w>4096 || h>4096) throw std::invalid_argument("logical size outside 1..4096");
            o.width = int(w); o.height = int(h);
        } else if (key == "--capture" && !value.empty()) o.capture = value;
        else throw std::invalid_argument("unsupported argument: " + std::string(key) + " " + std::string(value));
    }
    if (!o.capture.empty() && !o.frames) throw std::invalid_argument("--capture requires --frames");
    o.inspector=inspector.value_or(!o.frames);
    o.timing=timing.value_or(!o.frames);
    if (o.previews && !o.inspector) throw std::invalid_argument("--previews all requires --inspector on");
    o.display.validate();
    return o;
}

Action Controls::poll(Keys keys, bool active, double dt, Display& d, bool captured, Commands ui, bool freshFocus) {
    Action a;
    if (!active) {
        suspended_ = true;
        previous_.reset();
        clock_.reset();
        return a;
    }
    if (suspended_) {
        // A focus-aware backend has already canceled stale downs. Preserve new
        // presses received after gain, including those in the very same poll.
        blocked_ = freshFocus ? Keys{} : keys;
        suspended_ = false;
        dt = 0;
    }
    if (captured) blocked_ |= keys;
    blocked_ &= keys;
    keys &= ~blocked_;
    const Keys pressed = (keys & ~previous_) | ui.edges;
    previous_ = keys;
    a.exit = pressed[Escape];
    if (pressed[Bloom]) d.bloom = !d.bloom;
    if (pressed[Hud]) d.hud = !d.hud;
    if (pressed[ToggleLighting]) d.lighting = !d.lighting;
    if (!d.lighting) d.normalMapping=false;
    else if (pressed[ToggleNormal]) d.normalMapping=!d.normalMapping;
    for (int k=Scene; k<=Final; ++k)
        if (pressed[k]) d.view = static_cast<View>(k-Scene);
    d.strength = std::clamp(d.strength + pressed[Stronger] - pressed[Weaker],0,30);
    if (ui.strength) d.strength = std::clamp(*ui.strength,0,30);
    if (ui.view) d.view = *ui.view;
    if (pressed[ShowLight] && d.lighting) d.view=View::Light;
    if (pressed[ShowNormal] && d.normalMapping) d.view=View::Normal;
    if (!d.lighting && d.view==View::Light) d.view=View::Final;
    if (!d.normalMapping && d.view==View::Normal) d.view=View::Final;
    if (ui.ambient) d.ambient=*ui.ambient;
    if (ui.lightStrength) d.lightStrength=*ui.lightStrength;
    if (ui.radiusMultiplier) d.radiusMultiplier=*ui.radiusMultiplier;
    if (ui.normalStrength || ui.normalMaterial) {
        auto material=d.normalMaterial;
        if (ui.normalStrength) material.strength=*ui.normalStrength;
        if (ui.normalMaterial) material.kind=*ui.normalMaterial;
        material.validate();
        d.normalMaterial=material;
    }
    a.motion = {keys[Right]-keys[Left],keys[Down]-keys[Up],keys[Fire]};
    if (pressed[Reset]) {
        a.reset = true;
        clock_.reset();
    } else if (pressed[Pause]) {
        paused = !paused;
        clock_.reset();
    } else if (paused) a.steps = pressed[Step] ? 1 : 0;
    else a.steps = clock_.advance(dt);
    return a;
}

const std::array<tiny2d::Vector2,4> Neon::relays{{{150,-61},{235,72},{-174,94},{-232,-65}}};
Neon::Neon(std::uint32_t seed) : rng_(seed) {
    fire();
    bolts[0].age = 20;
    burst(relays[3]);
    bursts[0].age = 7;
}
void Neon::burst(tiny2d::Vector2 p) {
    rng_ = 1664525u * rng_ + 1013904223u;
    bursts[nextBurst_] = {p,0,float(rng_ >> 8) * (6.2831853f / 16777216.f),bursts[nextBurst_].generation+1};
    nextBurst_ = (nextBurst_+1) % bursts.size();
}
void Neon::fire() {
    bolts[nextBolt_] = {player,relays[nextRelay_],0,bolts[nextBolt_].generation+1};
    nextBolt_ = (nextBolt_+1) % bolts.size();
    nextRelay_ = (nextRelay_+1) % relays.size();
}
void Neon::update(Motion input) {
    ++tick;
    const float speed = input.x && input.y ? 2.4f * 0.70710678f : 2.4f;
    player.x = std::clamp(player.x+speed*input.x,-286.f,286.f);
    player.y = std::clamp(player.y+speed*input.y,-118.f,118.f);
    for (auto& b : bursts) if (b.age >= 0 && ++b.age >= flash) b.age = -1;
    for (auto& b : bolts) if (b.age >= 0 && ++b.age >= flight) {
        burst(b.end);
        b.age = -1;
    }
    if (tick % 32 == 0 || (input.fire && tick % 6 == 0)) fire();
}
}
