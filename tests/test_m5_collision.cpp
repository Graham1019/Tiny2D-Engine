#include "M5Test.h"
#include "tiny2d/Scene.h"
#include <limits>
using namespace tiny2d;
int main(){try{
 AABB b{{0,0},{2,2}};check(overlaps(b,b)&&overlaps(b,AABB{{2,2},{3,3}})&&!overlaps(b,AABB{{3,0},{4,1}}),"boxes");
 check(overlaps(Circle{{0,0},1},Circle{{2,0},1})&&!overlaps(Circle{{0,0},1},Circle{{3,0},1}),"circles");
 check(overlaps(b,Circle{{3,2},1})&&overlaps(Circle{{1,1},0},b)&&!overlaps(b,Circle{{3,3},1}),"mixed");
 check(overlaps(AABB{{0,0},{0,0}},Circle{{0,0},0}),"degenerate");
 rejects([&]{validate(AABB{{2,0},{1,1}});});rejects([&]{validate(Circle{{0,0},-1});});
 for(float v:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})rejects([&]{validate(Circle{{v,0},1});});
 float m=std::numeric_limits<float>::max();check(!overlaps(Circle{{-m,0},1},Circle{{m,0},1}),"double overflow guard");
 check(overlaps(worldShape(Circle{{1,1},0},{2,2}),Shape{Circle{{3,3},0}}),"world offset");
 Scene s;auto a=s.create(),c=s.create();s.find(a)->collider=b;s.find(c)->collider=b;std::vector<CollisionPhase> phases;
 auto cb=[&](Scene&,const CollisionEvent&e){phases.push_back(e.phase);};s.update(0,{},cb);s.update(0,{},cb);s.find(c)->transform.position={10,0};s.update(0,{},cb);
 check(phases==std::vector<CollisionPhase>({CollisionPhase::Enter,CollisionPhase::Stay,CollisionPhase::Exit}),"event phases");
 s.find(c)->transform.position={};s.update(0,{},[&](Scene&w,const CollisionEvent&){w.destroy(c);});check(s.contacts()==0&&!s.find(c),"delete contact");s.update(0,{},[&](Scene&,const CollisionEvent&){throw std::runtime_error("stale event");});

 auto d=s.create();s.find(d)->collider=b;s.find(d)->transform.rotation=1;s.find(d)->transform.scale={2,3};s.update(0);check(s.contacts()==1,"visual transform independent collider");s.find(d)->enabled=false;unsigned exits=0;s.update(0,{},[&](Scene&,const CollisionEvent&e){if(e.phase==CollisionPhase::Exit)++exits;});check(exits==1&&s.contacts()==0,"disabled exit");
 std::cout<<"Collision closed geometry and event tests passed\n";
}catch(const std::exception&e){std::cerr<<e.what();return 1;}}
