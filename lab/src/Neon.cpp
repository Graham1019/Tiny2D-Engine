#include "Neon.h"
#include "ParticleSimulation.h"
#include "tiny2d/MapGenerator.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <filesystem>
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
    case View::Shadow: return "shadow";
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
    if (!shadowLights || shadowLights>maxShadowLights)
        throw std::invalid_argument("--shadow-lights expects 1..8");
    if (shadows && !lighting) throw std::invalid_argument("shadows require lighting on");
    if (view==View::Shadow && !shadows) throw std::invalid_argument("Shadow view requires shadows on");
}
Options parse(int argc, const char* const* argv) {
    Options o;
    std::optional<bool> inspector, timing, lighting;
    bool particleOption=false, effectOption=false, mapOption=false, mapSize=false, navOption=false;
    bool showcaseOption=false,childFramesOption=false,explicitSize=false,shaderControlOption=false;
    bool seedOption=false, enemyOption=false, inputOption=false, timelineOption=false;
    for (int i=1; i<argc; ++i) {
        std::string_view key = argv[i];
        if (++i == argc) throw std::invalid_argument("missing value for " + std::string(key));
        std::string_view value = argv[i];
        if (key == "--scene") {
            if (value=="neon") o.scene=LabScene::Neon;
            else if (value=="particles") o.scene=LabScene::Particles;
            else if (value=="shaders") o.scene=LabScene::Shaders;
            else if (value=="map") o.scene=LabScene::Map;
            else if (value=="navigation") o.scene=LabScene::Navigation;
            else if (value=="replay") o.scene=LabScene::Replay;
            else if (value=="showcase") o.scene=LabScene::Showcase;
            else throw std::invalid_argument("--scene expects neon, particles, shaders, map, navigation, replay or showcase");
        } else if (key=="--showcase-exhibit" || key=="--showcase-tour" || key=="--showcase-output") {
            if (value.empty()) throw std::invalid_argument(std::string(key)+" requires a value");
            showcaseOption=true;
            if (key=="--showcase-exhibit") o.showcaseExhibit=value;
            else if (key=="--showcase-tour") o.showcaseTour=value;
            else o.showcaseOutput=value;
        } else if (key=="--shader-workspace" && !value.empty()) {
            o.shaderWorkspace=value;
        } else if (key=="--shader-control-stdin" && (value=="on" || value=="off")) {
            shaderControlOption=true;
            o.shaderControl=value=="on";
        } else if (key=="--child-frames") {
            showcaseOption=childFramesOption=true;
            o.childFrames=number(value,key);
            if (!o.childFrames || o.childFrames>3600) throw std::invalid_argument("--child-frames expects 1..3600");
        } else if (key=="--map-script") {
            if (value!="none" && value!="walk") throw std::invalid_argument("--map-script expects none or walk");
            mapOption=true;
            o.mapWalk=value=="walk";
        } else if (key=="--nav-enemies") {
            navOption=enemyOption=true;
            o.navEnemies=number(value,key);
            if (o.navEnemies>256) throw std::invalid_argument("--nav-enemies expects 0..256");
        } else if (key=="--nav-script") {
            navOption=inputOption=true;
            if (value!="none" && value!="walk") throw std::invalid_argument("--nav-script expects none or walk");
            o.navWalk=value=="walk";
        } else if (key=="--nav-choice") {
            navOption=inputOption=true;
            if (value!="first" && value!="last") throw std::invalid_argument("--nav-choice expects first or last");
            o.navChooseLast=value=="last";
        } else if (key=="--nav-debug") {
            navOption=true;
            if (value!="off" && value!="path" && value!="field" && value!="all")
                throw std::invalid_argument("--nav-debug expects off, path, field or all");
            o.navField=value=="field" || value=="all";
            o.navPath=value=="path" || value=="all";
        } else if (key=="--effect") {
            effectOption=true;
            const auto effect=std::find_if(galleryEffects.begin(),galleryEffects.end(),
                [&](auto item) { return value==galleryName(item); });
            if (effect==galleryEffects.end()) throw std::invalid_argument("--effect expects aurora, orbitals or contours");
            o.effect=*effect;
        } else if (key=="--particle-capacity" || key=="--particle-steady" || key=="--particle-rate") {
            particleOption=true;
            const auto n=number(value,key);
            if (key=="--particle-capacity") o.particleCapacity=n;
            else if (key=="--particle-steady") o.particleSteady=n;
            else o.particleRate=n;
        } else if (key=="--particle-script") {
            particleOption=true;
            if (value!="none" && value!="demo") throw std::invalid_argument("--particle-script expects none or demo");
            o.particleDemo=value=="demo";
        } else if (key == "--seed") {
            seedOption=true;
            o.seed=number(value,key);
        } else if (key=="--snapshot-in" || key=="--snapshot-out" || key=="--record" || key=="--replay" || key=="--branch-in" || key=="--branch-out") {
            if (value.empty()) throw std::invalid_argument(std::string(key)+" requires a path");
            if (key=="--snapshot-in") o.snapshotIn=value;
            if (key=="--snapshot-out") o.snapshotOut=value;
            if (key=="--record") o.recordPath=value;
            if (key=="--replay") o.replayPath=value;
            if (key=="--branch-in") o.branchIn=value;
            if (key=="--branch-out") o.branchOut=value;
        } else if (key=="--seek-event" || key=="--fork-at") {
            timelineOption=true;
            (key=="--seek-event"?o.seekEvent:o.forkAt)=number(value,key);
        } else if (key=="--branch-id") {
            timelineOption=true;
            o.branchId=number(value,key);
            if (!o.branchId) throw std::invalid_argument("--branch-id must be positive");
        } else if (key=="--timeline-view" && (value=="on" || value=="off")) {
            timelineOption=true;
            o.timelineView=value=="on";
        }
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
            else if (value == "shadow") o.display.view = View::Shadow;
            else throw std::invalid_argument("unknown --view");
        } else if (key == "--lighting" && (value == "on" || value == "off")) lighting=value=="on";
        else if (key == "--ambient") o.display.ambient=displayValue(value,key,Display::maxAmbient);
        else if (key == "--normal-mapping" && (value=="on" || value=="off")) o.display.normalMapping=value=="on";
        else if (key=="--shadows" && (value=="on" || value=="off")) o.display.shadows=value=="on";
        else if (key=="--shadow-lights") o.display.shadowLights=number(value,key);
        else if (key=="--occluders" && (value=="none" || value=="demo")) o.display.demoOccluders=value=="demo";
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
        else if (key == "--size" || key == "--map-size") {
            const auto split = value.find('x');
            if (split == value.npos) throw std::invalid_argument(std::string(key)+" expects WIDTHxHEIGHT");
            const auto w = number(value.substr(0,split),key), h = number(value.substr(split+1),key);
            if (key=="--map-size") {
                using Limits=tiny2d::MapParameters;
                if (w<Limits::minWidth || w>Limits::maxWidth || h<Limits::minHeight || h>Limits::maxHeight)
                    throw std::invalid_argument("--map-size expects 5..128 by 5..96 tiles");
                mapSize=true;
                o.mapWidth=int(w); o.mapHeight=int(h);
            } else {
                if (!w || !h || w>4096 || h>4096) throw std::invalid_argument("logical size outside 1..4096");
                explicitSize=true;
                o.width = int(w); o.height = int(h);
            }
        } else if (key == "--capture" && !value.empty()) o.capture = value;
        else throw std::invalid_argument("unsupported argument: " + std::string(key) + " " + std::string(value));
    }
    if (!o.shaderWorkspace.empty() && o.scene!=LabScene::Shaders && o.scene!=LabScene::Showcase)
        throw std::invalid_argument("--shader-workspace requires shaders or showcase");
    if (shaderControlOption && o.scene!=LabScene::Shaders)
        throw std::invalid_argument("--shader-control-stdin requires shaders");
    if (o.shaderControl && (!o.frames || *o.frames>3600 || o.shaderWorkspace.empty() || !o.capture.empty()))
        throw std::invalid_argument("shader stdin control requires 1..3600 frames, explicit workspace and per-command captures");
    if (showcaseOption && o.scene!=LabScene::Showcase)
        throw std::invalid_argument("showcase options require --scene showcase");
    if (o.scene==LabScene::Showcase) {
        if (!explicitSize) { o.width=960; o.height=580; }
        if (o.showcaseTour.empty() && (!o.showcaseOutput.empty() || childFramesOption))
            throw std::invalid_argument("--child-frames and --showcase-output require --showcase-tour");
        if (!o.showcaseTour.empty() && o.frames)
            throw std::invalid_argument("tour duration is controlled by --child-frames");
        if (!o.capture.empty() && !o.metrics.empty()) {
            namespace fs=std::filesystem;
            if (fs::weakly_canonical(fs::absolute(o.capture))==fs::weakly_canonical(fs::absolute(o.metrics)) ||
                (fs::exists(o.capture) && fs::exists(o.metrics) && fs::equivalent(o.capture,o.metrics)))
                throw std::invalid_argument("capture and metrics paths must differ");
        }
    }
    if (!o.capture.empty() && !o.frames && o.showcaseTour.empty()) throw std::invalid_argument("--capture requires --frames");
    o.inspector=inspector.value_or(!o.frames);
    o.timing=timing.value_or(!o.frames);
    if (o.previews && !o.inspector) throw std::invalid_argument("--previews all requires --inspector on");
    const bool particles=o.scene==LabScene::Particles;
    const bool shaders=o.scene==LabScene::Shaders;
    const bool map=o.scene==LabScene::Map;
    const bool replay=o.scene==LabScene::Replay;
    const bool navigation=o.scene==LabScene::Navigation || replay;
    const bool history=!o.replayPath.empty() || !o.branchIn.empty();
    const bool load=!o.snapshotIn.empty() || history;
    if (!replay && (load || !o.snapshotOut.empty() || !o.recordPath.empty() || timelineOption || !o.branchOut.empty()))
        throw std::invalid_argument("snapshot and replay file options require --scene replay");
    if (load && (mapSize || seedOption || enemyOption))
        throw std::invalid_argument("file supplies seed, map parameters and enemy count");
    if (!o.replayPath.empty() && !o.branchIn.empty())
        throw std::invalid_argument("choose one input history");
    if ((o.seekEvent || o.forkAt) && (!history || (o.seekEvent && o.forkAt)))
        throw std::invalid_argument("choose one seek or fork cursor with an input history");
    if ((!o.branchOut.empty() || o.branchId) && !o.forkAt)
        throw std::invalid_argument("--branch-out and --branch-id require --fork-at");
    if (history && ((inputOption && !o.forkAt) || !o.snapshotIn.empty() || !o.recordPath.empty()))
        throw std::invalid_argument("--replay supplies the base snapshot and all input events");
    if (replay) {
        for (const auto* output:{&o.snapshotOut,&o.recordPath,&o.branchOut}) {
            if (!output->empty() && std::filesystem::exists(std::filesystem::symlink_status(*output)))
                throw std::invalid_argument("output already exists: "+*output);
        }
        // Capture and Metrics historically replace their targets. Never allow those
        // output paths to alias an input snapshot, replay, or another output.
        const std::string* paths[]={&o.snapshotIn,&o.replayPath,&o.recordPath,&o.snapshotOut,&o.capture,&o.metrics,&o.branchIn,&o.branchOut};
        for (std::size_t i=0;i<std::size(paths);++i) {
            if (paths[i]->empty()) continue;
            for (std::size_t j=0;j<i;++j) {
                if (paths[j]->empty()) continue;
                namespace fs=std::filesystem;
                if (fs::weakly_canonical(fs::absolute(*paths[i]))==fs::weakly_canonical(fs::absolute(*paths[j])) ||
                    (fs::exists(*paths[i]) && fs::exists(*paths[j]) && fs::equivalent(*paths[i],*paths[j])))
                    throw std::invalid_argument("snapshot, replay, capture and metrics paths must be distinct");
            }
        }
    }
    if (mapSize && !map && !navigation) throw std::invalid_argument("--map-size requires --scene map, navigation or replay");
    if (navOption && !navigation) throw std::invalid_argument("navigation options require --scene navigation or replay");
    if (mapOption && !map) throw std::invalid_argument("map options require --scene map");
    o.display.lighting=lighting.value_or(particles);
    if (effectOption && !shaders) throw std::invalid_argument("--effect requires --scene shaders");
    if (shaders || map || navigation) {
        o.display.demoOccluders=false;
        if (o.display.lighting || o.display.normalMapping || o.display.shadows ||
            o.display.view==View::Light || o.display.view==View::Normal || o.display.view==View::Shadow)
            throw std::invalid_argument(std::string(navigation?"navigation":map?"map":"shader gallery")+" supports unlit HDR views scene, bright, blur and final");
    }
    if (particleOption && !particles) throw std::invalid_argument("particle options require --scene particles");
    if (!o.particleCapacity || o.particleCapacity>ParticleSimulation::maxCapacity ||
        o.particleSteady.value_or(0)>o.particleCapacity || o.particleRate>ParticleSimulation::maxRate)
        throw std::invalid_argument("particle capacity must be 1..8192, steady <= capacity, rate 0..60000");
    if (particles) {
        o.display.demoOccluders=false;
        if (o.display.normalMapping || o.display.shadows || o.display.view==View::Normal || o.display.view==View::Shadow)
            throw std::invalid_argument("particle scene has no normal material or shadow casters (views 6/7 unavailable)");
    }
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
    a.pressed=pressed;
    a.exit = pressed[Escape];
    if (pressed[Bloom]) d.bloom = !d.bloom;
    if (pressed[Hud]) d.hud = !d.hud;
    if (pressed[ToggleLighting]) d.lighting = !d.lighting;
    if (!d.lighting) {
        d.normalMapping=false;
        d.shadows=false;
    } else {
        if (pressed[ToggleNormal]) d.normalMapping=!d.normalMapping;
        if (pressed[ToggleShadows]) d.shadows=!d.shadows;
    }
    for (int k=Scene; k<=Final; ++k)
        if (pressed[k]) d.view = static_cast<View>(k-Scene);
    d.strength = std::clamp(d.strength + pressed[Stronger] - pressed[Weaker],0,30);
    if (ui.strength) d.strength = std::clamp(*ui.strength,0,30);
    if (ui.view) d.view = *ui.view;
    if (pressed[ShowLight] && d.lighting) d.view=View::Light;
    if (pressed[ShowNormal] && d.normalMapping) d.view=View::Normal;
    if (pressed[ShowShadow] && d.shadows) d.view=View::Shadow;
    if (!d.lighting && d.view==View::Light) d.view=View::Final;
    if (!d.normalMapping && d.view==View::Normal) d.view=View::Final;
    if (!d.shadows && d.view==View::Shadow) d.view=View::Final;
    if (ui.shadowLights) d.shadowLights=std::clamp(*ui.shadowLights,1u,Display::maxShadowLights);
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
