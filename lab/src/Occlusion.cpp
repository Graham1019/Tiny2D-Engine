#include "Occlusion.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lab {
namespace {
using tiny2d::Vector2;
constexpr double contactEpsilon=1.e-7; // World units, inclusive at both segment ends.
constexpr double minFeature=1.e-5;
struct Point { double x,y; };
Point point(Vector2 v) { return {v.x,v.y}; }
Point operator-(Point a,Point b) { return {a.x-b.x,a.y-b.y}; }
double cross(Point a,Point b) { return a.x*b.y-a.y*b.x; }
double side(Point a,Point b,Point p) { return cross(b-a,p-a); }
double length(Point v) { return std::hypot(v.x,v.y); }

void validatePoint(Vector2 p) {
    if (!(std::abs(p.x)<=1.e6f && std::abs(p.y)<=1.e6f))
        throw std::invalid_argument("occlusion coordinates must be finite and within +/-1e6 world units");
}
void validateEdge(Vector2 a,Vector2 b) {
    validatePoint(a); validatePoint(b);
    if (length(point(b)-point(a))<minFeature)
        throw std::invalid_argument("degenerate occluder edge");
}
bool touches(Point p,Point a,Point b) {
    const Point d=b-a, q=p-a;
    const double l=length(d);
    if (l==0) return length(q)<=contactEpsilon;
    const double t=std::clamp((q.x*d.x+q.y*d.y)/(l*l),0.,1.);
    return std::hypot(q.x-t*d.x,q.y-t*d.y)<=contactEpsilon;
}
bool intersects(Point a,Point b,Point c,Point d) {
    if (touches(a,c,d) || touches(b,c,d) || touches(c,a,b) || touches(d,a,b)) return true;
    return side(a,b,c)*side(a,b,d)<0 && side(c,d,a)*side(c,d,b)<0;
}

// Clip in light-local double precision. Opposite signs at a crossing guarantee a nonzero denominator.
struct Polygon { std::array<Point,7> v; unsigned n; };
template<class HalfPlane>
Polygon clip(const Polygon& in,HalfPlane distance) {
    Polygon out{{},0};
    if (!in.n) return out;
    Point prev=in.v[in.n-1];
    double a=distance(prev);
    for (unsigned i=0;i<in.n;++i) {
        const Point cur=in.v[i];
        const double b=distance(cur);
        if ((a<0 && b>0) || (a>0 && b<0)) {
            const double t=a/(a-b);
            out.v[out.n++]={prev.x+(cur.x-prev.x)*t,prev.y+(cur.y-prev.y)*t};
        }
        if (b>=0) out.v[out.n++]=cur;
        prev=cur; a=b;
    }
    return out;
}
void appendFan(ShadowMesh& mesh,const Polygon& poly,Point origin) {
    auto append=[&](Point v) { mesh.vertices[mesh.count++]={float(v.x+origin.x),float(v.y+origin.y)}; };
    for (unsigned i=2;i<poly.n;++i) {
        append(poly.v[0]); append(poly.v[i-1]); append(poly.v[i]);
    }
}
}

void OccluderList::checkCapacity(unsigned id,std::size_t edges) const {
    if (count_==maxOccluders || edges>maxEdges-edgeCount_)
        throw std::length_error("visual occluder or edge budget exhausted");
    if (std::any_of(begin(),end(),[&](const Occluder2D& object) { return object.id==id; }))
        throw std::invalid_argument("duplicate occluder identity");
}
void OccluderList::addSegment(unsigned id,Vector2 a,Vector2 b) {
    validateEdge(a,b);
    checkCapacity(id,1);
    objects_[count_++]={id,edgeCount_,1};
    edges_[edgeCount_++]={a,b};
}
void OccluderList::addRect(unsigned id,Vector2 origin,Vector2 size) {
    if (!(size.x>0 && size.y>0)) throw std::invalid_argument("invalid occluder rectangle size");
    const Vector2 opposite=origin+size;
    addConvex(id,{origin,{opposite.x,origin.y},opposite,{origin.x,opposite.y}});
}
void OccluderList::addConvex(unsigned id,std::initializer_list<Vector2> vertices) {
    checkCapacity(id,vertices.size());
    const auto n=static_cast<unsigned>(vertices.size());
    if (n<3) throw std::invalid_argument("a convex occluder needs at least three vertices");
    const auto* v=vertices.begin();
    for (unsigned i=0;i<n;++i) {
        const unsigned next=(i+1)%n;
        validateEdge(v[i],v[next]);
        const Point a=point(v[i]), b=point(v[next]);
        const double areaTolerance=minFeature*length(b-a);
        // Checking every other vertex against every directed edge also rejects stars and bow ties.
        for (unsigned j=0;j<n;++j) {
            if (j!=i && j!=next && !(side(a,b,point(v[j]))>areaTolerance))
                throw std::invalid_argument("occluder must be strictly convex, nondegenerate and counterclockwise");
        }
    }
    objects_[count_++]={id,edgeCount_,n};
    for (unsigned i=0;i<n;++i) edges_[edgeCount_++]={v[i],v[(i+1)%n]};
}
const OccluderList& demoOccluders() {
    static const auto walls=[] {
        OccluderList out;
        // The first wall crosses the overlap between Player cyan and the southwest amber relay.
        out.addRect(1,{-95,20},{8,29});
        out.addSegment(2,{106,-71},{128,-48});
        return out;
    }();
    return walls;
}
bool isOccluded(Vector2 light,Vector2 receiver,const OccluderList& walls) {
    validatePoint(light); validatePoint(receiver);
    const Point l=point(light), p=point(receiver);
    for (const auto& wall:walls) {
        bool inside=wall.solid();
        for (unsigned j=0;j<wall.edgeCount;++j) {
            const auto& e=walls.edge(wall.firstEdge+j);
            const Point a=point(e.a), b=point(e.b);
            if (intersects(l,p,a,b)) return true;
            if (side(a,b,l)<0) inside=false;
        }
        if (inside) return true;
    }
    return false;
}
ShadowMesh projectShadow(const Light2D& light,const OccluderList& walls) {
    light.validate();
    ShadowMesh mesh;
    const Point origin=point(light.position);
    const double r=light.radius;
    const Polygon support{{Point{-r,-r},{r,-r},{r,r},{-r,r}},4};
    for (const auto& wall:walls) {
        bool inside=wall.solid();
        for (unsigned j=0;j<wall.edgeCount;++j) {
            const auto& e=walls.edge(wall.firstEdge+j);
            Point a=point(e.a)-origin, b=point(e.b)-origin;
            // For CCW solids the light sees an entry face only from its right half-plane.
            const double facing=cross(a,b);
            if (touches({0,0},a,b)) {
                mesh.count=0; appendFan(mesh,support,origin); return mesh;
            }
            if (facing<0) inside=false;
            if (facing==0 || (wall.solid() && facing>0)) continue;
            if (facing<0) std::swap(a,b);
            // Intersection of angular aperture, region behind the edge and finite light support box.
            Polygon wedge=clip(support,[&](Point p) { return cross(a,p); });
            wedge=clip(wedge,[&](Point p) { return cross(p,b); });
            wedge=clip(wedge,[&](Point p) { return -side(a,b,p); });
            appendFan(mesh,wedge,origin);
        }
        if (inside) {
            mesh.count=0; appendFan(mesh,support,origin); return mesh;
        }
    }
    return mesh;
}

bool ShadowCasters::contains(unsigned index) const {
    return std::find(indices.begin(),indices.begin()+count,index)!=indices.begin()+count;
}
const Light2D* ShadowCasters::find(const LightList& lights,LabHandle source) const {
    for (unsigned i=0;i<count;++i) {
        const auto index=indices[i];
        if (index<lights.size() && lights.begin()[index].source==source) return lights.begin()+index;
    }
    return nullptr;
}
ShadowCasters selectShadowCasters(const LightList& lights,unsigned budget) {
    if (!budget || budget>ShadowCasters::capacity) throw std::invalid_argument("shadow light budget outside 1..8");
    ShadowCasters result;
    for (auto kind:{ObjectKind::Player,ObjectKind::Relay,ObjectKind::Bolt,ObjectKind::Burst}) {
        for (unsigned i=0;i<lights.size() && result.count<budget;++i)
            if (lights.begin()[i].source.kind==kind) result.indices[result.count++]=i;
    }
    result.skipped=lights.size()-result.count;
    return result;
}
}
