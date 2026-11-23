#include "Selection.h"
#include <cmath>
#include <limits>

namespace lab {
using tiny2d::Vector2;
bool LabHandle::operator==(const LabHandle& h) const {
    return epoch==h.epoch && kind==h.kind && slot==h.slot && generation==h.generation;
}
const char* name(ObjectKind kind) {
    switch(kind) {
    case ObjectKind::Player: return "Player";
    case ObjectKind::Relay: return "Relay";
    case ObjectKind::Bolt: return "Bolt";
    case ObjectKind::Burst: return "Burst";
    }
    return "Unknown";
}
Vector2 along(const Bolt& b,float t) {
    return {b.start.x+(b.end.x-b.start.x)*t,b.start.y+(b.end.y-b.start.y)*t};
}
float burstRadius(float t,bool shortRay) { return 9+t*(shortRay?27:40); }
namespace {
std::optional<Object> snapshot(const Neon& n,std::uint64_t epoch,ObjectKind kind,unsigned slot) {
    LabHandle id{epoch,kind,slot,1};
    switch (kind) {
    case ObjectKind::Player:
        if (!slot) return Object{id,n.player,24};
        break;
    case ObjectKind::Relay:
        if (slot<n.relays.size()) return Object{id,n.relays[slot],19};
        break;
    case ObjectKind::Bolt:
        if (slot<n.bolts.size() && n.bolts[slot].age>=0) {
            const auto& b=n.bolts[slot];
            id.generation=b.generation;
            return Object{id,along(b,float(b.age)/n.flight),4,b.start,b.end,b.age,n.flight};
        }
        break;
    case ObjectKind::Burst:
        if (slot<n.bursts.size() && n.bursts[slot].age>=0) {
            const auto& b=n.bursts[slot];
            const float t=float(b.age)/n.flash;
            id.generation=b.generation;
            return Object{id,b.center,burstRadius(t,false)+8*(1-t),{},{},b.age,n.flash,b.angle};
        }
        break;
    }
    return {};
}
}
std::optional<Object> resolve(const Neon& n,std::uint64_t epoch,LabHandle id) {
    if (id.epoch!=epoch) return {};
    auto object=snapshot(n,epoch,id.kind,id.slot);
    if (object && object->id==id) return object;
    return {};
}
Objects objects(const Neon& n,std::uint64_t epoch) {
    Objects out;
    out.reserve(1+n.relays.size()+n.bolts.size()+n.bursts.size());
    auto add=[&](ObjectKind kind,std::size_t count) {
        for (unsigned i=0;i<count;++i)
            if (auto obj=snapshot(n,epoch,kind,i)) out.push_back(*obj);
    };
    add(ObjectKind::Player,1); add(ObjectKind::Relay,n.relays.size());
    add(ObjectKind::Bolt,n.bolts.size()); add(ObjectKind::Burst,n.bursts.size());
    return out;
}
std::optional<LabHandle> Presented::pick(Vector2 p,Content current, View v,
        std::uint64_t gen,const Neon& n,std::uint64_t epoch) const {
    if (!frame || gen!=generation || v!=view || (v!=View::Scene && v!=View::Final)
        || current.x!=rect.x || current.y!=rect.y || current.size.width!=rect.size.width
        || current.size.height!=rect.size.height) return {};
    p.x-=rect.x; p.y-=rect.y;
    if (!(p.x>=0 && p.y>=0 && p.x<rect.size.width && p.y<rect.size.height)) return {};
    p=camera.viewportToWorld(p);
    const Object* winner=nullptr;
    float best=std::numeric_limits<float>::infinity();
    for (const auto& obj:objects) {
        float distance=std::hypot(p.x-obj.position.x,p.y-obj.position.y);
        if (distance>obj.radius+3/camera.zoom()) continue;
        if (!winner || obj.id.kind>winner->id.kind ||
            (obj.id.kind==winner->id.kind && (distance<best ||
             (distance==best && obj.id.slot<winner->id.slot)))) {
            winner=&obj; best=distance;
        }
    }
    if (winner && resolve(n,epoch,winner->id)) return winner->id;
    return {};
}
}
