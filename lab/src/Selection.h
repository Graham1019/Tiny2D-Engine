#pragma once
#include "Neon.h"
#include "tiny2d/Camera2D.h"
#include "tiny2d/Window.h"
#include <vector>

namespace lab {
// Ascending world drawing order breaks ties between overlapping semantic shapes.
enum class ObjectKind { Relay, Bolt, Burst, Player };
const char* name(ObjectKind);
struct LabHandle {
    std::uint64_t epoch;
    ObjectKind kind;
    unsigned slot;
    std::uint64_t generation;
    bool operator==(const LabHandle& other) const;
};
struct Object {
    LabHandle id;
    tiny2d::Vector2 position;
    float radius;
    tiny2d::Vector2 start{}, end{};
    int age=-1, lifetime=0;
    float angle=0;
};
using Objects=std::vector<Object>;
tiny2d::Vector2 along(const Bolt&,float progress);
float burstRadius(float progress,bool shortRay);
std::optional<Object> resolve(const Neon&,std::uint64_t epoch,LabHandle);
Objects objects(const Neon&,std::uint64_t epoch);
struct Content { float x=0,y=0; tiny2d::Size size{}; };
struct Presented {
    Objects objects;
    tiny2d::Camera2D camera{1,1};
    Content rect;
    View view=View::Final;
    std::uint64_t generation=0,frame=0,tick=0;
    std::optional<LabHandle> pick(tiny2d::Vector2 cursor,Content current,
        View currentView,std::uint64_t currentGeneration,const Neon&,std::uint64_t epoch) const;
};
}
