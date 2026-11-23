#pragma once

#include "Lighting.h"
#include <array>
#include <initializer_list>

namespace lab {

struct OccluderEdge { tiny2d::Vector2 a, b; };
struct Occluder2D {
    unsigned id, firstEdge, edgeCount;
    bool solid() const { return edgeCount > 1; }
};

// World-space, visual-only geometry. Identities and edges are read-only after insertion.
class OccluderList {
public:
    static constexpr unsigned maxOccluders=16, maxEdges=64;
    unsigned size() const { return count_; }
    unsigned edgeCount() const { return edgeCount_; }
    const Occluder2D* begin() const { return objects_.data(); }
    const Occluder2D* end() const { return begin()+count_; }
    const OccluderEdge& edge(unsigned i) const { return edges_.at(i); }
    void clear() { count_=edgeCount_=0; }
    void addSegment(unsigned id,tiny2d::Vector2 a,tiny2d::Vector2 b);
    void addRect(unsigned id,tiny2d::Vector2 origin,tiny2d::Vector2 size);
    // Strictly convex, counterclockwise in numerical XY; omit the repeated first vertex.
    void addConvex(unsigned id,std::initializer_list<tiny2d::Vector2> vertices);
private:
    void checkCapacity(unsigned id,std::size_t edges) const;
    std::array<Occluder2D,maxOccluders> objects_{};
    std::array<OccluderEdge,maxEdges> edges_{};
    unsigned count_=0,edgeCount_=0;
};
const OccluderList& demoOccluders();

struct ShadowMesh {
    // A square clipped by three half-planes has at most seven vertices: five fan triangles.
    static constexpr unsigned capacity=OccluderList::maxEdges*15;
    std::array<tiny2d::Vector2,capacity> vertices;
    unsigned count=0;
};
ShadowMesh projectShadow(const Light2D&,const OccluderList&);
// Independent closed finite-segment reference. An emitter touching or inside a wall is blocked.
bool isOccluded(tiny2d::Vector2 light,tiny2d::Vector2 receiver,const OccluderList&);

struct ShadowCasters {
    static constexpr unsigned capacity=8;
    std::array<unsigned,capacity> indices{};
    unsigned count=0,skipped=0;
    bool contains(unsigned lightIndex) const;
    const Light2D* find(const LightList&,LabHandle) const;
};
ShadowCasters selectShadowCasters(const LightList&,unsigned budget);
}
