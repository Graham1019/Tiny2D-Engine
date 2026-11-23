#include "Lighting.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lab {
namespace {
bool inRange(float value,float low,float high) {
    return std::isfinite(value) && value>=low && value<=high;
}
}
void Light2D::validate() const {
    if (!inRange(position.x,-1e6f,1e6f) || !inRange(position.y,-1e6f,1e6f) ||
        !inRange(radius,1.f/1024,4096) || !inRange(intensity,0,32) ||
        !inRange(color.r,0,1) || !inRange(color.g,0,1) || !inRange(color.b,0,1) || color.a!=1)
        throw std::invalid_argument("invalid point light position, radius, linear color or intensity");
}
void LightList::add(const Light2D& light) {
    light.validate();
    if (count_==capacity) ++dropped_;
    else data_[count_++]=light;
}
const Light2D* LightList::find(LabHandle source) const {
    auto it=std::find_if(begin(),end(),[&](const Light2D& l){return l.source==source;});
    return it==end() ? nullptr : it;
}
float attenuation(const Light2D& light,tiny2d::Vector2 p) {
    light.validate();
    if (!std::isfinite(p.x) || !std::isfinite(p.y))
        throw std::invalid_argument("non-finite light sample position");
    const double dx=(double(p.x)-light.position.x)/light.radius;
    const double dy=(double(p.y)-light.position.y)/light.radius;
    const double weight=std::max(0.,1.-dx*dx-dy*dy);
    return float(weight*weight);
}
LightList extractLights(const Neon& n,const Display& d,std::uint64_t epoch) {
    d.validate();
    LightList lights;
    if (!d.lighting) return lights;
    auto add=[&](ObjectKind kind,unsigned slot,std::uint64_t generation,tiny2d::Vector2 p,
                 float radius,tiny2d::Color color,float strength) {
        lights.add({p,radius*d.radiusMultiplier,color,strength*d.lightStrength,{epoch,kind,slot,generation}});
    };
    add(ObjectKind::Player,0,1,n.player,92,{.055f,.82f,1,1},3.4f);
    for (unsigned i=0;i<n.relays.size();++i)
        add(ObjectKind::Relay,i,1,n.relays[i],76,{1,.49f,.085f,1},3.3f);
    for (unsigned i=0;i<n.bolts.size();++i) {
        const auto& b=n.bolts[i];
        if (b.age>=0 && b.age<n.flight)
            add(ObjectKind::Bolt,i,b.generation,along(b,float(b.age)/n.flight),
                46,{1,.225f,.035f,1},5.f);
    }
    for (unsigned i=0;i<n.bursts.size();++i) {
        const auto& b=n.bursts[i];
        if (b.age>=0 && b.age<n.flash) {
            const float remaining=1-float(b.age)/n.flash;
            add(ObjectKind::Burst,i,b.generation,b.center,89,{.77f,.095f,1,1},7.f*remaining*remaining);
        }
    }
    return lights;
}
}
