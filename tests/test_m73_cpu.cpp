#include "Lighting.h"
#include "Presentation.h"
#include "M5Test.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace lab {
struct NeonProbe {
    static auto hidden(const Neon& n) {return std::make_tuple(n.rng_,n.nextBolt_,n.nextBurst_,n.nextRelay_);}
};
}

namespace {
using namespace lab;
constexpr auto nan=std::numeric_limits<float>::quiet_NaN();
constexpr auto inf=std::numeric_limits<float>::infinity();
bool samePosition(tiny2d::Vector2 a,tiny2d::Vector2 b) {return a.x==b.x && a.y==b.y;}
bool sameLight(const Light2D& a,const Light2D& b) {
    return samePosition(a.position,b.position) && a.radius==b.radius && a.intensity==b.intensity &&
           a.color.r==b.color.r && a.color.g==b.color.g && a.color.b==b.color.b && a.source==b.source;
}
void boundedData() {
    Light2D light{{5,-4},8,{1,.5f,.25f,1},32,{}};
    check(attenuation(light,light.position)==1 && attenuation(light,{9,-4})==.5625f &&
          attenuation(light,{5,4})==0 && attenuation(light,{5,44})==0,"bounded squared falloff");
    for (int i=-100;i<=100;++i) {
        const auto value=light.color.g*light.intensity*attenuation(light,{5.f+i/8.f,-4});
        check(std::isfinite(value) && value>=0 && value<=16,"finite nonnegative contribution");
    }
    check(attenuation(light,{std::numeric_limits<float>::max(),0})==0,"extreme exterior point");
    rejects([&]{attenuation(light,{nan,0});});
    LightList lights;
    for (unsigned slot=0;slot<40;++slot) {
        light.source.slot=slot;
        lights.add(light);
    }
    check(lights.size()==32 && lights.dropped()==8,"light budget and overflow count");
    unsigned count=0;
    for (const auto& current:lights) check(current.source.slot==count++,"first 32 in insertion order");
    for (float bad:{nan,inf,-1.f,0.f,1e-25f,4097.f}) {
        auto invalid=light;
        invalid.radius=bad;
        rejects([&]{lights.add(invalid);});
    }
    for (float bad:{nan,inf,-1.f,33.f}) {
        auto invalid=light;
        invalid.intensity=bad;
        rejects([&]{lights.add(invalid);});
    }
    for (float bad:{nan,inf,-.1f,1.01f}) {
        auto invalid=light;
        invalid.color.r=bad;
        rejects([&]{lights.add(invalid);});
    }
    for (float bad:{nan,inf,1.1e6f}) {
        auto invalid=light;
        invalid.position.y=bad;
        rejects([&]{lights.add(invalid);});
    }
    auto invalid=light;
    invalid.color.a=nan;
    rejects([&]{lights.add(invalid);});
    check(lights.size()==32 && lights.dropped()==8,"invalid candidate leaves snapshot unchanged");
}
void equalSimulation(const Neon& a,const Neon& b) {
    check(a.tick==b.tick && NeonProbe::hidden(a)==NeonProbe::hidden(b) && samePosition(a.player,b.player),
          "tick, player, RNG and circular indices");
    for (unsigned i=0;i<a.bolts.size();++i) {
        const auto& x=a.bolts[i]; const auto& y=b.bolts[i];
        check(x.age==y.age && x.generation==y.generation && samePosition(x.start,y.start) &&
              samePosition(x.end,y.end),"bolt lifetime, identity and path");
    }
    for (unsigned i=0;i<a.bursts.size();++i) {
        const auto& x=a.bursts[i]; const auto& y=b.bursts[i];
        check(x.age==y.age && x.generation==y.generation && x.angle==y.angle &&
              samePosition(x.center,y.center),"burst lifetime, identity, position and RNG angle");
    }
}
void simulation() {
    Display on,off;
    on.lighting=true;
    on.hud=off.hud=false;
    Neon n(71),reference(71);
    Picture lit,legacy;
    const LabHandle firstBolt{19,ObjectKind::Bolt,0,n.bolts[0].generation};
    const LabHandle firstBurst{19,ObjectKind::Burst,0,n.bursts[0].generation};
    float previousBurst=std::numeric_limits<float>::max();
    for (unsigned tick=0;tick<180;++tick) {
        if (tick%19==0) on.radiusMultiplier=on.radiusMultiplier==1 ? 2 : 1;
        lit.prepare(n,on,false,{800,450},{1600,900},{},19);
        legacy.prepare(reference,off,false,{800,450},{1600,900},{},19);
        const auto repeat=extractLights(reference,on,19);
        check(lit.lights.size()==repeat.size() && std::equal(lit.lights.begin(),lit.lights.end(),repeat.begin(),sameLight),
              "same seed, input and display regenerate the same ordered lights");
        const auto list=objects(n,19);
        check(lit.lights.size()==list.size() && !lit.lights.dropped(),"one current light per live source");
        for (const auto& object:list) {
            const auto* l=lit.lights.find(object.id);
            check(l && samePosition(l->position,object.position),"true current source position and generation");
            switch (object.id.kind) {
            case ObjectKind::Player: check(l->radius==92*on.radiusMultiplier && l->color.b>l->color.r,"cyan player"); break;
            case ObjectKind::Relay: check(l->color.r>l->color.g && l->color.g>l->color.b,"amber relays"); break;
            case ObjectKind::Bolt: check(l->color.r>l->color.g && l->color.g>l->color.b,"orange bolt"); break;
            case ObjectKind::Burst: check(l->color.b>l->color.g && l->color.r>l->color.g,"purple burst"); break;
            }
        }
        if (tick<20) {
            auto burst=lit.lights.find(firstBurst);
            check(burst && burst->intensity<previousBurst,"burst fades with remaining lifetime");
            previousBurst=burst->intensity;
        } else check(!lit.lights.find(firstBurst),"expired or recycled burst cannot be selected");
        if (tick>=24) check(!lit.lights.find(firstBolt),"expired or recycled bolt cannot be selected");
        check(lit.world.empty() && !lit.albedo.empty() && !lit.emission.empty() && legacy.emission.empty() &&
              legacy.albedo.empty() && !legacy.world.empty() && !legacy.lights.size(),"material paths are exclusive");
        if (!tick) {
            const auto surfaces=lit.albedo.size();
            lit.lightRadii(firstBolt);
            check(lit.debug.size()>=lit.lights.size()*3 && lit.albedo.size()==surfaces,"post-process light overlay geometry");
        }
        Motion motion{int(tick%3)-1,int((tick/9)%3)-1,true};
        n.update(motion); reference.update(motion);
        equalSimulation(n,reference);
    }
    check(n.bolts[0].generation>firstBolt.generation && n.bursts[0].generation>firstBurst.generation,"exercised slot reuse");
    n=Neon(71); equalSimulation(n,Neon(71));
    const auto reset=extractLights(n,on,20);
    check(!reset.find(firstBolt) && reset.find({20,ObjectKind::Bolt,0,firstBolt.generation}),"reset epoch invalidates selection");
    const auto original=extractLights(n,on,19);
    on.lightStrength=Display::maxLightStrength;
    auto scaled=extractLights(n,on,19);
    check(scaled.find(firstBolt)->intensity==original.find(firstBolt)->intensity*on.lightStrength,"global multiplier");
    for (auto& bolt:n.bolts) bolt.age=0;
    for (auto& burst:n.bursts) burst.age=0;
    check(extractLights(n,on,20).size()==25,"all possible Neon slots fit in budget");
}

}
int main() {
    try { boundedData(); simulation(); }
    catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
