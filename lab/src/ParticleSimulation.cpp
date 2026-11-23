#include "ParticleSimulation.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace lab {
namespace {
using tiny2d::Vector2;
constexpr float radians=0.01745329251994329577f;

bool inRange(float value,float low,float high) {
    return value>=low && value<=high; // also rejects NaNs and infinities
}
bool inArena(Vector2 v) {
    return inRange(v.x,-ParticleSimulation::halfWidth,ParticleSimulation::halfWidth) &&
           inRange(v.y,-ParticleSimulation::halfHeight,ParticleSimulation::halfHeight);
}
unsigned validCapacity(unsigned n) {
    if (!n || n>ParticleSimulation::maxCapacity)
        throw std::invalid_argument("particle capacity outside 1..8192");
    return n;
}
void validate(const ParticleEmitter& e) {
    if (!inArena(e.position) || e.rate>ParticleSimulation::maxRate || !e.lifetime || e.lifetime>3600 ||
        !inRange(e.speed,0,ParticleSimulation::maxSpeed) || !inRange(e.direction,-360,360) ||
        !inRange(e.spread,0,180) || !inRange(e.size,.25f,40) || !inRange(e.emission,0,32) ||
        !inRange(e.color.r,0,4) || !inRange(e.color.g,0,4) || !inRange(e.color.b,0,4))
        throw std::invalid_argument("particle emitter parameter outside supported range");
}
void validate(const ForceField2D& f) {
    if (f.kind<FieldKind::Attractor || f.kind>FieldKind::Wind || !inArena(f.position) ||
        !inRange(f.radius,1,640) || !inRange(f.strength,0,100000) ||
        !inRange(f.direction.x,-1,1) || !inRange(f.direction.y,-1,1))
        throw std::invalid_argument("particle force field parameter outside supported range");
}
ParticleFields defaultFields() {
    ParticleFields fields{};
    fields[0]={FieldKind::Attractor,true,{112,-32},144,500,false,{1,0}};
    fields[1]={FieldKind::Repulsor,true,{-94,55},94,680,false,{1,0}};
    fields[2]={FieldKind::Vortex,true,{-55,-30},128,600,false,{1,0}};
    fields[3]={FieldKind::Wind,true,{},100,14,false,{1,.18f}};
    return fields;
}
void limit(Vector2& v,float magnitude) {
    const float squared=v.x*v.x+v.y*v.y;
    if (squared>magnitude*magnitude) {
        const float scale=magnitude/std::sqrt(squared);
        v.x*=scale; v.y*=scale;
    }
}
void reflect(float& position,float& velocity,float extent) {
    if (position>extent) { position=2*extent-position; velocity=-velocity; }
    else if (position < -extent) { position=-2*extent-position; velocity=-velocity; }
}
// Precompute constants once per enabled field. Radial direction uses softened
// distance; compact support uses true distance so acceleration vanishes at radius.
struct PreparedField {
    ForceField2D field;
    float radiusSquared, inverseRadius, epsilonSquared;
    Vector2 wind;
};
PreparedField prepare(const ForceField2D& f) {
    const float epsilon=std::max(.25f,.025f*f.radius);
    PreparedField result{f,f.radius*f.radius,1.f/f.radius,epsilon*epsilon,{}};
    if (f.kind==FieldKind::Wind) {
        const double length=std::hypot(double(f.direction.x),double(f.direction.y));
        if (length>0) result.wind={float(f.direction.x/length*f.strength),float(f.direction.y/length*f.strength)};
    }
    return result;
}
Vector2 acceleration(const PreparedField& prepared,Vector2 position) {
    const auto& f=prepared.field;
    if (f.kind==FieldKind::Wind) return prepared.wind;
    const float dx=f.position.x-position.x, dy=f.position.y-position.y;
    const float squared=dx*dx+dy*dy;
    if (squared>=prepared.radiusSquared) return {};
    const float weight=std::max(0.f,1.f-std::sqrt(squared)*prepared.inverseRadius);
    const float scale=f.strength*weight*weight/std::sqrt(squared+prepared.epsilonSquared);
    if (f.kind==FieldKind::Attractor) return {dx*scale,dy*scale};
    if (f.kind==FieldKind::Repulsor) return {-dx*scale,-dy*scale};
    // World +Y is down. At the right edge, clockwise points down.
    const float sign=f.clockwise ? 1.f : -1.f;
    return {sign*dy*scale,-sign*dx*scale};
}
}

ParticleRenderSnapshot::ParticleRenderSnapshot(unsigned capacity) {
    instances.reserve(validCapacity(capacity));
}
ParticleSimulation::ParticleSimulation(std::uint32_t seed,unsigned capacity,unsigned rate,std::optional<unsigned> steady)
    :seed_(seed),initialRate_(rate),steady_(steady),slots_(validCapacity(capacity)),accelerations_(capacity) {
    if (rate>maxRate || (steady && *steady>capacity))
        throw std::invalid_argument("particle rate or steady target outside supported range");
    free_.reserve(capacity);
    reset();
}
void ParticleSimulation::reset() {
    if (epoch_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("particle epoch exhausted");
    ++epoch_;
    rng_=seed_;
    emitter_=ParticleEmitter{};
    emitter_.rate=initialRate_;
    fields_=defaultFields();
    tick_=dropped_=active_=remainder_=0;
    std::fill(slots_.begin(),slots_.end(),Particle2D{});
    free_.clear();
    for (unsigned i=capacity();i>0;--i) free_.push_back(i-1);
}
float ParticleSimulation::unit() {
    rng_=1664525u*rng_+1013904223u;
    return static_cast<float>(rng_>>8u)*(1.f/16777216.f);
}
void ParticleSimulation::spawn() {
    const unsigned slot=free_.back();
    free_.pop_back();
    auto& p=slots_[slot];
    // Every successful birth consumes exactly six LCG outputs in this order.
    const float x=unit(), y=unit(), heading=unit(), speed=unit(), life=unit(), tint=unit();
    const float angle=(emitter_.direction+(2*heading-1)*emitter_.spread)*radians;
    const float magnitude=emitter_.speed*(.75f+.25f*speed);
    p.position={std::clamp(emitter_.position.x+(2*x-1)*3.f,-halfWidth,halfWidth),
                std::clamp(emitter_.position.y+(2*y-1)*3.f,-halfHeight,halfHeight)};
    p.velocity={magnitude*std::cos(angle),magnitude*std::sin(angle)};
    p.age=0;
    p.lifetime=emitter_.lifetime-static_cast<unsigned>(life*(emitter_.lifetime/5+1));
    p.size=emitter_.size;
    p.emission=emitter_.emission;
    // Cool cyan / warm amber / lilac populations reveal intersecting streams.
    const ParticleRGB palette=tint<.44f ? ParticleRGB{.18f,1.f,.94f} :
                              tint<.72f ? ParticleRGB{1.f,.36f,.07f} : ParticleRGB{.83f,.26f,1.f};
    p.color={emitter_.color.r*palette.r,emitter_.color.g*palette.g,emitter_.color.b*palette.b};
    ++p.generation;
    p.active=true;
    ++active_;
}
double ParticleSimulation::step(const ParticleTickInput& input) {
    if (input.emitter) validate(*input.emitter);
    if (input.fields) for (const auto& f:*input.fields) validate(f);
    if (input.burst>maxBurst) throw std::invalid_argument("particle burst outside 0..512");
    if (tick_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("particle tick exhausted");

    if (input.emitter) emitter_=*input.emitter;
    if (input.fields) fields_=*input.fields;
    for (unsigned i=0;i<capacity();++i) {
        auto& p=slots_[i];
        if (p.active && ++p.age==p.lifetime) {
            p.active=false;
            --active_;
            free_.push_back(i);
        }
    }
    unsigned requested=input.burst;
    if (steady_) requested=*steady_-active_;
    else if (emitter_.enabled) {
        remainder_+=emitter_.rate;
        requested+=remainder_/60;
        remainder_%=60;
    }
    const unsigned accepted=std::min(requested,capacity()-active_);
    const unsigned rejected=requested-accepted;
    // Saturation preserves the monotonic count for arbitrarily long sessions.
    dropped_+=std::min<std::uint64_t>(rejected,std::numeric_limits<std::uint64_t>::max()-dropped_);
    for (unsigned i=0;i<accepted;++i) spawn();

    using Clock=std::chrono::steady_clock;
    const auto forceStart=Clock::now();
    if (active_) {
        std::array<PreparedField,8> enabled;
        unsigned count=0;
        for (const auto& field:fields_) if (field.enabled) enabled[count++]=prepare(field);
        for (unsigned i=0;i<capacity();++i) {
            if (!slots_[i].active) continue;
            Vector2 sum{};
            for (unsigned j=0;j<count;++j) {
                const auto a=acceleration(enabled[j],slots_[i].position);
                sum.x+=a.x; sum.y+=a.y;
            }
            limit(sum,maxAcceleration);
            accelerations_[i]=sum;
        }
    }
    const auto forceEnd=Clock::now();
    if (active_) for (unsigned i=0;i<capacity();++i) {
        auto& p=slots_[i];
        if (!p.active) continue;
        p.velocity.x=damping*(p.velocity.x+accelerations_[i].x*dt);
        p.velocity.y=damping*(p.velocity.y+accelerations_[i].y*dt);
        limit(p.velocity,maxSpeed);
        p.position.x+=p.velocity.x*dt;
        p.position.y+=p.velocity.y*dt;
        reflect(p.position.x,p.velocity.x,halfWidth);
        reflect(p.position.y,p.velocity.y,halfHeight);
    }
    ++tick_;
    return std::chrono::duration<double,std::micro>(forceEnd-forceStart).count();
}
unsigned ParticleSimulation::activeFields() const {
    return static_cast<unsigned>(std::count_if(fields_.begin(),fields_.end(),[](const auto& f){return f.enabled;}));
}
ParticleHandle ParticleSimulation::handle(unsigned slot) const {
    if (slot>=capacity() || !slots_[slot].active) throw std::out_of_range("inactive particle slot");
    return {epoch_,slot,slots_[slot].generation};
}
const Particle2D* ParticleSimulation::resolve(ParticleHandle h) const {
    if (h.epoch!=epoch_ || h.slot>=capacity()) return nullptr;
    const auto& p=slots_[h.slot];
    return p.active && p.generation==h.generation ? &p : nullptr;
}
void ParticleSimulation::snapshot(ParticleRenderSnapshot& output) const {
    if (output.instances.capacity()<capacity())
        throw std::length_error("particle snapshot must reserve simulation capacity");
    output.instances.clear();
    if (active_) for (const auto& p:slots_) if (p.active)
        output.instances.push_back({p.position.x,p.position.y,p.size,float(p.age)/p.lifetime,
                                    p.color.r,p.color.g,p.color.b,p.emission});
}
}
