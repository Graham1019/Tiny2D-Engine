#include "M5Test.h"
#include "Neon.h"
#include "ParticleSimulation.h"

#include <cmath>
#include <limits>
#include <tuple>

namespace lab {
struct ParticleSimulationProbe {
    static const auto& freeSlots(const ParticleSimulation& s) { return s.free_; }
    static unsigned rng(const ParticleSimulation& s) { return s.rng_; }
    static unsigned remainder(const ParticleSimulation& s) { return s.remainder_; }
};
}
namespace {
using namespace lab;
using tiny2d::Vector2;
using Probe=ParticleSimulationProbe;

bool same(Vector2 a,Vector2 b) { return a.x==b.x && a.y==b.y; }
bool same(ParticleRGB a,ParticleRGB b) { return a.r==b.r && a.g==b.g && a.b==b.b; }
bool near(float a,float b) { return std::abs(a-b)<.0001f; }
void equalState(const ParticleSimulation& a,const ParticleSimulation& b) {
    check(a.tick()==b.tick() && a.capacity()==b.capacity() && a.active()==b.active() &&
          a.dropped()==b.dropped() && Probe::rng(a)==Probe::rng(b) &&
          Probe::freeSlots(a)==Probe::freeSlots(b) && Probe::remainder(a)==Probe::remainder(b),
          "counter, RNG, free stack and integer remainder");
    const auto& x=a.emitter(); const auto& y=b.emitter();
    check(x.enabled==y.enabled && same(x.position,y.position) && x.rate==y.rate &&
          x.lifetime==y.lifetime && x.speed==y.speed && x.direction==y.direction &&
          x.spread==y.spread && x.size==y.size && x.emission==y.emission && same(x.color,y.color),
          "all emitter fields");
    for (unsigned i=0;i<a.fields().size();++i) {
        const auto& f=a.fields()[i]; const auto& g=b.fields()[i];
        check(f.kind==g.kind && f.enabled==g.enabled && same(f.position,g.position) &&
              f.radius==g.radius && f.strength==g.strength && f.clockwise==g.clockwise &&
              same(f.direction,g.direction),"all force fields");
    }
    for (unsigned i=0;i<a.capacity();++i) {
        const auto& p=a.slots()[i]; const auto& q=b.slots()[i];
        check(p.active==q.active && p.generation==q.generation && p.age==q.age &&
              p.lifetime==q.lifetime && p.size==q.size && p.emission==q.emission &&
              same(p.color,q.color) && same(p.position,q.position) && same(p.velocity,q.velocity),
              "all active and retired slot fields");
    }
}
void pool() {
    ParticleSimulation defaults;
    check(defaults.capacity()==4096 && defaults.activeFields()==4,"defaults");
    rejects([]{ParticleSimulation invalid(1,0);});
    rejects([]{ParticleSimulation invalid(1,8193);});
    rejects([]{ParticleSimulation invalid(1,5,1,6);});
    rejects([]{ParticleSimulation invalid(1,1,60001);});
    rejects([]{ParticleRenderSnapshot invalid(8193);});
    ParticleSimulation s(0,3,0);
    const auto initialEpoch=s.epoch();
    check(Probe::freeSlots(s)==std::vector<unsigned>{2,1,0},"ascending first allocation");
    auto e=s.emitter(); e.lifetime=1; e.spread=0; e.direction=0;
    e.speed=120; e.position={20,30};
    ParticleTickInput input{e,ParticleFields{},5};
    s.step(input);
    check(s.active()==3 && !s.activeFields() && s.dropped()==2 && Probe::freeSlots(s).empty(),
          "bounded spawn and exact overflow");
    std::uint32_t rng=0;
    auto unit=[&] { rng=rng*1664525u+1013904223u; return float(rng>>8u)/16777216.f; };
    for (const auto& p:s.slots()) {
        const float birthX=20+(2*unit()-1)*3;
        const float birthY=30+(2*unit()-1)*3;
        unit();
        const float velX=e.speed*(.75f+.25f*unit())*ParticleSimulation::damping;
        unit(); unit();
        check(p.active && p.age==0 && p.lifetime==1 && p.generation==1 &&
              near(p.position.x,birthX+velX*ParticleSimulation::dt) && near(p.position.y,birthY) &&
              near(p.velocity.x,velX) && p.velocity.y==0,"newborn integrated once in allocation order");
    }
    check(Probe::rng(s)==rng,"rejected births consume no RNG");
    const auto first=s.handle(0); const auto last=s.handle(2);
    check(s.resolve(first)==&s.slots()[0] && !s.resolve({s.epoch(),3,1}),"handle lookup by slot");
    s.step({{},{},1});
    check(s.active()==1 && !s.resolve(first) && !s.resolve(last) && s.slots()[2].active &&
          s.slots()[2].generation==2 && Probe::freeSlots(s)==std::vector<unsigned>{0,1},
          "ascending deaths, last released first reused");
    rejects([&]{s.handle(0);});
    const auto reused=s.handle(2);
    s.reset();
    check(s.epoch()!=initialEpoch && !s.resolve(reused) && !s.tick() && !s.dropped() &&
          !s.active() && Probe::rng(s)==0,"reset clears pool and seed-zero LCG");
    s.step(input);
    check(!s.resolve(first) && s.handle(0).generation==first.generation,"epoch retires equal-generation slot");
    ParticleSimulation steady(19,8192,60000,8192);
    auto shortLife=steady.emitter(); shortLife.lifetime=1; shortLife.enabled=false;
    for (int j=0;j<8;++j) {
        steady.step({shortLife,{},512});
        check(steady.active()==8192 && steady.dropped()==0 && steady.slots()[0].generation==unsigned(j+1),
              "steady target replenishes exactly after simultaneous deaths");
    }
    ParticleSimulation empty(19,8192,60000,0);
    for (int j=0;j<8;++j) empty.step({{},{},512});
    check(!empty.active() && !empty.dropped() && Probe::rng(empty)==19,"zero steady suppresses rate and bursts");
}
void transaction() {
    ParticleSimulation s(18,64,37), ref(18,64,37);
    for (int i=0;i<2;++i) {s.step(); ref.step();}
    const auto epoch=s.epoch();
    const auto unchanged=[&](ParticleTickInput bad) {
        rejects([&]{s.step(bad);});
        equalState(s,ref);
        check(s.epoch()==epoch,"invalid input cannot invalidate handles");
    };
    for (float bad:{std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                    std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::max()}) {
        for (auto member:{&ParticleEmitter::direction,&ParticleEmitter::speed,&ParticleEmitter::size,
                          &ParticleEmitter::spread,&ParticleEmitter::emission}) {
            auto e=s.emitter(); e.*member=bad;
            unchanged({e,s.fields(),1});
        }
        auto e=s.emitter(); e.position.x=bad; unchanged({e,{},0});
        e=s.emitter(); e.color.b=bad; unchanged({e,{},0});
        e=s.emitter(); e.rate=8;
        auto f=s.fields(); f.back().strength=bad; unchanged({e,f,1});
        f=s.fields(); f.back().direction.y=bad; unchanged({e,f,1});
        f=s.fields(); f.back().radius=bad; unchanged({e,f,1});
        f=s.fields(); f.back().position.y=bad; unchanged({e,f,1});
    }
    auto f=s.fields(); f[0].radius=0; unchanged({{},f,0});
    f=s.fields(); f[0].kind=static_cast<FieldKind>(90); unchanged({{},f,0});
    auto e=s.emitter(); e.lifetime=0; unchanged({e,{},0});
    e=s.emitter(); e.rate=60001; unchanged({e,{},0});
    unchanged({s.emitter(),s.fields(),513});
    ParticleRenderSnapshot small(1);
    rejects([&]{s.snapshot(small);});
    equalState(s,ref);
}
struct Trial { Vector2 velocity,position; };
Trial fieldTrial(FieldKind kind,Vector2 relative,Vector2 direction={1,0},bool clockwise=false,float strength=600,
                 Vector2 origin={0,0}) {
    ParticleSimulation s(0,1,0);
    auto emitter=s.emitter(); emitter.position=origin; emitter.speed=0;
    ParticleFields fields{};
    s.step({emitter,fields,1});
    const auto p=s.slots()[0].position;
    fields[0]={kind,true,{p.x+relative.x,p.y+relative.y},80,strength,clockwise,direction};
    s.step({{},fields,0});
    return {s.slots()[0].velocity,s.slots()[0].position};
}
void forces() {
    const auto a=fieldTrial(FieldKind::Attractor,{-8,0}).velocity;
    check(a.x<0 && a.y==0,"attraction toward center");
    const auto r=fieldTrial(FieldKind::Repulsor,{-8,0}).velocity;
    check(r.x==-a.x && r.y==0,"repulsion away from center");
    const auto clockwise=fieldTrial(FieldKind::Vortex,{-8,0},{},true).velocity;
    const auto counter=fieldTrial(FieldKind::Vortex,{-8,0},{},false).velocity;
    check(clockwise.x==0 && clockwise.y>0 && counter.x==0 && counter.y==-clockwise.y,
          "world +Y down vortex convention");
    for (auto kind:{FieldKind::Vortex,FieldKind::Attractor,FieldKind::Repulsor}) {
        check(same(fieldTrial(kind,{0,0}).velocity,{0,0}),"center is finite and stationary");
        check(same(fieldTrial(kind,{80,0}).velocity,{0,0}),"compact support edge");
        const auto nearCenter=fieldTrial(kind,{.000001f,0}).velocity;
        check(std::isfinite(nearCenter.x) && std::isfinite(nearCenter.y),"near-center finite");
    }
    const auto w=fieldTrial(FieldKind::Wind,{0,0},{.3f,.4f}).velocity;
    check(same(w,fieldTrial(FieldKind::Wind,{-140,60},{.3f,.4f},false,600,{200,-80}).velocity) &&
          w.x>0 && near(w.y/w.x,4.f/3.f),"global normalized wind ignores position and radius");
    check(same(fieldTrial(FieldKind::Wind,{0,0},{0,0}).velocity,{0,0}),"zero wind direction means calm");
    const auto capped=fieldTrial(FieldKind::Wind,{0,0},{1,1},false,100000).velocity;
    check(near(std::hypot(capped.x,capped.y),ParticleSimulation::maxAcceleration*ParticleSimulation::dt*
               ParticleSimulation::damping),"extreme valid strength hits acceleration cap");
    ParticleSimulation s(11,12,0,12);
    auto e=s.emitter(); e.position={320,160}; e.speed=350; e.direction=45; e.spread=0;
    ParticleFields f{};
    for (auto& field:f) field={FieldKind::Wind,true,{},1,100000,false,{1,1}};
    s.step({e,f,0});
    bool hitCap=false;
    for (unsigned i=0;i<600;++i) {
        s.step();
        for (const auto& p:s.slots()) {
            const float speed=std::hypot(p.velocity.x,p.velocity.y);
            check(std::abs(p.position.x)<=320 && std::abs(p.position.y)<=160 && speed<=350.0001f,
                  "reflected positions and summed eight-field speed are bounded");
            hitCap=hitCap || speed>349.999f;
        }
    }
    check(hitCap,"reached max speed during boundary exercise");
}
ParticleTickInput script(const ParticleSimulation& s) {
    ParticleTickInput input;
    if (s.tick()%11==0) input.burst=3;
    if (s.tick()%33==0) {
        auto f=s.fields(); f[2].clockwise=!f[2].clockwise;
        input.fields=f;
    }
    if (s.tick()%59==0) {
        auto e=s.emitter(); e.lifetime=40; e.direction=float(s.tick()%240);
        input.emitter=e;
    }
    return input;
}
void timingAndSnapshot() {
    ParticleSimulation slow(33,80,137),fast(33,80,137), replay(33,80,137);
    Controls a,b; Display da,db;
    auto poll=[](ParticleSimulation& s,Controls& controls,Display& display,Keys keys,double elapsed) {
        const auto action=controls.poll(keys,true,elapsed,display);
        for (unsigned i=0;i<action.steps;++i) s.step(script(s));
    };
    ParticleRenderSnapshot snap(80);
    const auto* allocation=snap.instances.data();
    for (int i=0;i<120;++i) {
        poll(slow,a,da,{},1./30);
        for (unsigned j=0;j<4;++j) poll(fast,b,db,{},1./120);
        equalState(slow,fast);
        slow.snapshot(snap);
        check(snap.instances.size()==slow.active() && snap.instances.data()==allocation,"one reserved snapshot");
        unsigned instance=0;
        for (const auto& p:slow.slots()) if (p.active) {
            const auto& q=snap.instances.at(instance++);
            check(same({q.x,q.y},p.position) && q.size==p.size && q.ageFraction==float(p.age)/p.lifetime &&
                  q.ageFraction>=0 && q.ageFraction<1 && q.r==p.color.r && q.g==p.color.g && q.b==p.color.b &&
                  q.emission==p.emission,"exact finite linear snapshot in stable slot order");
        }
        equalState(slow,fast);
    }
    check(slow.tick()==240,"same number of fixed ticks at 30 and 120 Hz");
    const auto oldEpoch=slow.epoch();
    slow.reset();
    check(slow.epoch()!=oldEpoch,"new epoch");
    equalState(slow,replay);
    for (unsigned i=0;i<240;++i) { slow.step(script(slow)); replay.step(script(replay)); equalState(slow,replay); }
    equalState(slow,fast);
    Keys pause,step; pause.set(Pause); step.set(Step);
    poll(fast,b,db,pause,.75);
    for (int i=0;i<5;++i) poll(fast,b,db,{},.5);
    equalState(slow,fast);
    poll(fast,b,db,step,.5);
    slow.step(script(slow)); equalState(slow,fast);
    poll(fast,b,db,step,1.);
    equalState(slow,fast);
    check(b.paused && fast.tick()==241,"single Step edge advances exactly once");
    ParticleSimulation rate(12,128,31);
    for (unsigned i=1;i<=60;++i) {
        rate.step();
        check(rate.active()==i*31/60 && Probe::remainder(rate)==(i*31)%60,"integer emission remainder");
    }
    auto e=rate.emitter(); e.enabled=false; e.rate=60000;
    rate.step({e,{},2});
    check(rate.active()==33 && Probe::remainder(rate)==0,"disable regular births while allowing explicit burst");
}
}
int main() {
    try { pool(); transaction(); forces(); timingAndSnapshot(); }
    catch (const std::exception& e) {std::cerr<<e.what()<<'\n'; return 1;}
}
