#include "M5Test.h"
#include "Presentation.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace lab {
struct NeonProbe {
    static auto cursors(const Neon& n) { return std::make_tuple(n.rng_,n.nextBolt_,n.nextBurst_,n.nextRelay_); }
};
}

namespace {
using namespace lab;
using tiny2d::Vector2;
bool same(Vector2 a,Vector2 b) { return a.x==b.x && a.y==b.y; }
Light2D lamp(Vector2 xy,float radius=32) {
    return {xy,radius,{1,1,1,1},1,{1,ObjectKind::Player,0,1}};
}
void validation() {
    const float nan=std::numeric_limits<float>::quiet_NaN(), inf=std::numeric_limits<float>::infinity();
    OccluderList walls;
    for (float bad:{nan,inf,-inf,1.e20f}) {
        rejects([&]{ walls.addSegment(1,{bad,0},{1,2}); });
        rejects([&]{ walls.addRect(1,{0,0},{1,bad}); });
        rejects([&]{ isOccluded({0,0},{1,bad},walls); });
    }
    rejects([&]{ walls.addSegment(1,{0,0},{0,0}); });
    rejects([&]{ walls.addSegment(1,{0,0},{1.e-9f,1.e-9f}); });
    rejects([&]{ walls.addRect(1,{3,0},{-1,1}); });
    rejects([&]{ walls.addRect(1,{3,0},{2,0}); });
    rejects([&]{ walls.addConvex(1,{{0,0},{0,2},{2,0}}); }); // clockwise
    rejects([&]{ walls.addConvex(1,{{0,0},{2,0},{1,0}}); });
    rejects([&]{ walls.addConvex(1,{{0,0},{2,2},{0,2},{2,0}}); }); // bow tie
    rejects([&]{ walls.addConvex(1,{{0,0},{2,0},{1,.5f},{2,2},{0,2}}); }); // dent
    rejects([&]{ walls.addConvex(1,{{0,0},{2,0},{0,1.e-7f}}); });
    check(!walls.size() && !walls.edgeCount(),"invalid walls leave list unchanged");

    walls.addConvex(4,{{0,0},{2,0},{1,2}});
    rejects([&]{ walls.addSegment(4,{0,0},{1,0}); });
    check(walls.size()==1 && walls.edgeCount()==3 && walls.begin()->id==4 &&
          same(walls.edge(2).a,{1,2}) && same(walls.edge(2).b,{0,0}),"directed closing edge and immutable identity");
    walls.clear();
    for (unsigned i=0;i<OccluderList::maxOccluders;++i) walls.addSegment(i,{float(i),0},{float(i),1});
    rejects([&]{ walls.addSegment(100,{0,3},{1,3}); });
    check(walls.size()==16 && walls.edgeCount()==16 && walls.begin()[15].id==15,"first 16 retained");
    walls.clear();
    for (unsigned i=0;i<12;++i)
        walls.addConvex(i,{{0,0},{2,0},{3,1},{2,2},{0,2}});
    rejects([&]{ walls.addConvex(90,{{0,0},{2,0},{3,1},{2,2},{0,2}}); });
    walls.addRect(91,{0,0},{1,1});
    rejects([&]{ walls.addSegment(92,{0,0},{1,1}); });
    check(walls.size()==13 && walls.edgeCount()==64 && walls.begin()[12].id==91,"edge budget independent of object budget");
}
double side(Vector2 a,Vector2 b,Vector2 p) {
    return (double(b.x)-a.x)*(double(p.y)-a.y)-(double(b.y)-a.y)*(double(p.x)-a.x);
}
bool covers(const ShadowMesh& mesh,Vector2 p) {
    for (unsigned i=0;i<mesh.count;i+=3) {
        const auto a=mesh.vertices[i], b=mesh.vertices[i+1], c=mesh.vertices[i+2];
        if (side(a,b,c)==0) continue;
        const auto ab=side(a,b,p),bc=side(b,c,p),ca=side(c,a,p);
        if ((ab>=0 && bc>=0 && ca>=0) || (ab<=0 && bc<=0 && ca<=0)) return true;
    }
    return false;
}
void bounds(const ShadowMesh& mesh,const Light2D& l) {
    check(mesh.count<=ShadowMesh::capacity && mesh.count%3==0,"bounded triangle list");
    for (unsigned i=0;i<mesh.count;++i) {
        const auto v=mesh.vertices[i];
        check(std::abs(v.x-l.position.x)<=l.radius+.1f && std::abs(v.y-l.position.y)<=l.radius+.1f,
              "finite projected vertices clipped to light support box");
    }
}
void geometry() {
    OccluderList segment;
    segment.addSegment(5,{0,-4},{0,4});
    const Vector2 left{-10,0}, right{10,0};
    check(isOccluded(left,right,segment) && isOccluded(right,left,segment),"two-sided finite segment");
    check(!isOccluded(left,{-2,0},segment) && !isOccluded(left,{1,12},segment),"before and past the endpoint");
    check(isOccluded(left,{10,8},segment) && isOccluded(left,{0,4},segment) &&
          !isOccluded(left,{10,8.001f},segment),"closed ray, closed wall endpoints and near miss");
    check(isOccluded({0,7},{0,-7},segment) && !isOccluded({0,7},{0,6},segment),"collinear intervals");
    check(isOccluded({0,4},{0,20},segment) && !isOccluded(left,left,segment),"light touching endpoint and zero ray");
    const auto almostCollinear=lamp({-.0003f,5});
    bounds(projectShadow(almostCollinear,segment),almostCollinear);
    const auto almostCoincident=lamp({-1.e-9f,4});
    check(covers(projectShadow(almostCoincident,segment),{5,5}),"near-coincident emitter uses conservative contact rule");
    OccluderList walls;
    walls.addRect(9,{0,-4},{3,8});
    walls.addSegment(10,{4,-1},{4,6});
    walls.addConvex(11,{{-8,5},{-3,5},{-4,9},{-7,11}});
    check(isOccluded(left,{2,0},walls) && isOccluded({2,0},{2,1},walls),"solid entry and interior");
    check(isOccluded(left,right,walls),"overlapping shadows form a union");
    for (auto xy:{left,right,Vector2{0,0},Vector2{1,1},Vector2{0,-4},Vector2{-.0001f,-3.99f},Vector2{15,11}}) {
        const auto light=lamp(xy,24);
        const auto mesh=projectShadow(light,walls);
        bounds(mesh,light);
        for (int y=-20;y<=20;y+=2)
            for (int x=-20;x<=20;x+=2) {
                const Vector2 receiver{xy.x+x+.172f,xy.y+y+.313f};
                check(covers(mesh,receiver)==isOccluded(xy,receiver,walls),"projected mesh versus closed ray intersection");
            }
        check(!covers(mesh,{xy.x+25,xy.y}),"geometry clipped beyond light support");
    }
    const auto fromLeft=projectShadow(lamp(left),segment), fromRight=projectShadow(lamp(right),segment);
    check(covers(fromLeft,{5,0}) && !covers(fromLeft,{-5,0}) &&
          !covers(fromRight,{5,0}) && covers(fromRight,{-5,0}),"moving across wall swaps both samples");
    OccluderList huge;
    huge.addSegment(1,{-1.e6f,-1.e6f},{1.e6f,1.e6f});
    huge.addRect(2,{999990,999990},{10,10});
    for (float r:{1.f/1024,4096.f})
        for (auto p:{Vector2{999997,999997},Vector2{999984,999983},Vector2{-999998,-999997}})
            bounds(projectShadow(lamp(p,r),huge),lamp(p,r));
    rejects([&]{ projectShadow(lamp({2.e6f,0}),walls); });
}
void selection() {
    LightList list;
    for (unsigned i=0;i<40;++i) {
        auto light=lamp({0,0});
        const ObjectKind kinds[]={ObjectKind::Burst,ObjectKind::Bolt,ObjectKind::Relay,ObjectKind::Player};
        light.source={3,kinds[i%4],i,19};
        list.add(light);
    }
    rejects([&]{ selectShadowCasters(list,0); });
    rejects([&]{ selectShadowCasters(list,9); });
    for (unsigned budget=1;budget<=8;++budget) {
        auto s=selectShadowCasters(list,budget);
        check(s.count==budget && s.skipped==32-budget && list.size()==32 && list.dropped()==8,
              "independent lighting and shadow budgets");
        for (unsigned i=0;i<s.count;++i) check(s.indices[i]==3+4*i,"Player priority and stable list order");
        unsigned unshadowed=0;
        for (unsigned i=0;i<list.size();++i)
            if (!s.contains(i) && attenuation(list.begin()[i],{0,0})>0) ++unshadowed;
        check(unshadowed==s.skipped,"budget-excluded lights retained with full intensity");
    }
    const auto old=selectShadowCasters(list,8);
    const auto id=list.begin()[3].source;
    check(old.find(list,id)==list.begin()+3,"stable handle lookup");
    check(!old.find(list,{3,id.kind,id.slot,20}) && !old.find(list,{4,id.kind,id.slot,19}),
          "generation and epoch both required");
    Display d;
    d.lighting=true;
    Neon n(71);
    const auto lights=extractLights(n,d,2);
    const auto known=selectShadowCasters(lights,8);
    const auto bolt=lights.begin()[5].source, burst=lights.begin()[6].source;
    check(known.find(lights,bolt) && known.find(lights,burst),"initial dynamic sources selected");
    for (unsigned i=0;i<130;++i) n.update({0,0,true});
    const auto later=extractLights(n,d,2);
    check(n.bolts[bolt.slot].generation>bolt.generation && n.bursts[burst.slot].generation>burst.generation &&
          !selectShadowCasters(later,8).find(later,bolt) && !selectShadowCasters(later,8).find(later,burst),
          "newborns reusing both slots cannot inherit inspection");
    const auto reset=extractLights(Neon(71),d,3);
    check(!selectShadowCasters(reset,8).find(reset,lights.begin()->source),"reset retires Player epoch");
}
void simulation() {
    Neon a(71),b(71);
    Display on,off;
    on.lighting=off.lighting=true; on.shadows=true;
    on.hud=off.hud=false;
    Picture shadows,original;
    for (unsigned i=0;i<180;++i) {
        shadows.prepare(a,on,false,{800,450},{1600,900},{},7);
        original.prepare(b,off,false,{800,450},{1600,900},{},7);
        check(shadows.occluders.size()==2 && shadows.occluders.edgeCount()==5 && original.occluders.size()==0,
              "visual walls exist only in enabled scene");
        auto active=selectShadowCasters(shadows.lights,4);
        auto replayLights=extractLights(b,on,7);
        auto replay=selectShadowCasters(replayLights,4);
        check(active.indices==replay.indices && active.skipped==replay.skipped,"repeatable caster snapshot");
        for (unsigned j=0;j<active.count;++j) {
            const auto& light=shadows.lights.begin()[active.indices[j]];
            const auto& copy=replayLights.begin()[replay.indices[j]];
            const auto mesh=projectShadow(light,shadows.occluders), repeat=projectShadow(copy,demoOccluders());
            check(light.source==copy.source && mesh.count==repeat.count &&
                  std::equal(mesh.vertices.begin(),mesh.vertices.begin()+mesh.count,repeat.vertices.begin(),same),
                  "same seed/input/epoch reproduce selected mesh");
        }
        if (!i) {
            const Vector2 sample{-120,49};
            const auto& player=shadows.lights.begin()[active.indices[0]];
            const auto& relay=shadows.lights.begin()[active.indices[3]];
            check(player.source.kind==ObjectKind::Player && relay.source.kind==ObjectKind::Relay &&
                  isOccluded(player.position,sample,shadows.occluders) && !isOccluded(relay.position,sample,shadows.occluders) &&
                  attenuation(player,sample)>0 && attenuation(relay,sample)>0 && relay.color.r>player.color.r,
                  "demo uses genuinely overlapping cyan Player and amber Relay radii");
        }
        const Motion m{int(i%7)/3-1,int(i%11)/4-1,true};
        a.update(m); b.update(m);
        check(a.tick==b.tick && same(a.player,b.player) && NeonProbe::cursors(a)==NeonProbe::cursors(b),
              "same tick, player, RNG and allocation cursors");
        for (unsigned j=0;j<a.bolts.size();++j) {
            const auto& x=a.bolts[j]; const auto& y=b.bolts[j];
            check(x.age==y.age && x.generation==y.generation && same(x.start,y.start) && same(x.end,y.end),
                  "unchanged bolt trajectory, age, generation");
        }
        for (unsigned j=0;j<a.bursts.size();++j) {
            const auto& x=a.bursts[j]; const auto& y=b.bursts[j];
            check(x.age==y.age && x.generation==y.generation && same(x.center,y.center) && x.angle==y.angle,
                  "unchanged burst lifetime, identity and RNG rotation");
        }
    }
    on.demoOccluders=false;
    shadows.prepare(a,on,false,{800,450},{1600,900},{},7);
    original.prepare(b,off,false,{800,450},{1600,900},{},7);
    check(!shadows.occluders.size() && shadows.albedo.size()==original.albedo.size() &&
          shadows.emission.size()==original.emission.size(),"none omits all wall sprites");
}
}
int main() {
    try { validation(); geometry(); selection(); simulation(); }
    catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
