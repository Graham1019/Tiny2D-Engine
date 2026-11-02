#include "M5Test.h"
#include "tiny2d/Scene.h"
#include <type_traits>
#include <new>
#include <cstdlib>
namespace { int allocationFailure=-1; }
void* operator new(std::size_t size){
 if(allocationFailure==0){allocationFailure=-1;throw std::bad_alloc();}
 if(allocationFailure>0)--allocationFailure;
 if(void* p=std::malloc(size?size:1))return p;throw std::bad_alloc();
}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
static_assert(!std::is_copy_assignable_v<tiny2d::GameObject>);
using namespace tiny2d;
int main(){try{
 // Exercise GameObject, slot-vector and creation-order allocation failures.
 for(int point=0;point<3;++point){Scene failed;bool caught=false;allocationFailure=point;try{failed.create();}catch(const std::bad_alloc&){caught=true;}allocationFailure=-1;check(caught&&failed.count()==0,"failed create published object");auto valid=failed.create();check(failed.find(valid)&&failed.count()==1,"create recovery");}
 Scene s;auto a=s.create();check(s.find(a)&&s.find(a)->id()==a&&s.count()==1,"create");check(s.destroy(a)&&!s.destroy(a)&&!s.find(a),"destroy");auto b=s.create();check(a.index==b.index&&a.generation!=b.generation,"generation");check(!s.find({}),"invalid");
 s.find(b)->enabled=false;unsigned calls=0;s.update(1,[&](Scene&,ObjectId,double){++calls;});check(!calls,"disabled");s.find(b)->enabled=true;
 ObjectId c;s.update(0,[&](Scene&world,ObjectId id,double){c=world.create();world.destroy(id);++calls;});check(calls==1&&s.count()==1&&!s.find(b),"mutation snapshot");
 auto d=s.create();s.find(c)->sprite=SpriteData{};s.find(d)->sprite=SpriteData{};check(s.renderables()==std::vector<ObjectId>({c,d}),"stable render order");
 s.clear();check(!s.find(c)&&!s.find(d)&&s.count()==0,"clear");
 for(int round=0;round<20;++round){std::vector<ObjectId> ids;for(int i=0;i<1000;++i)ids.push_back(s.create());auto pointer=s.find(ids.front());s.create();check(pointer==s.find(ids.front()),"stable storage");s.clear();for(auto id:ids)check(!s.find(id),"stress stale");}
 auto id=s.create();rejects([&]{s.update(0,[](Scene&,ObjectId,double){throw std::runtime_error("callback");});});check(s.destroy(id),"exception recovery");

 Scene other;auto foreign=other.create();auto local=s.create();check(!(foreign==local)&&foreign.index==local.index,"scene identity");check(!s.find(foreign)&&!other.find(local)&&!s.destroy(foreign)&&!other.destroy(local),"foreign handles");
 auto token=local.sceneId;s.clear();auto fresh=s.create();check(fresh.sceneId==token&&!s.find(local),"clear identity");
 alignas(Scene) unsigned char storage[sizeof(Scene)];auto first=new(storage) Scene;auto expired=first->create();first->~Scene();auto second=new(storage) Scene;auto replacement=second->create();check(!second->find(expired)&&replacement.sceneId!=expired.sceneId,"address reuse");second->~Scene();
 rejects([&]{s.update(0,[&](Scene&w,ObjectId id,double){w.destroy(id);throw std::runtime_error("destroy callback");});});check(!s.find(fresh)&&s.count()==0,"exception collection");
 s.create();s.create();unsigned visited=0;s.update(0,[&](Scene&w,ObjectId,double){++visited;w.clear();});check(visited==1&&s.count()==0,"clear during update");
 for(Scene* world:{&s,&other}){
  world->clear();auto x=world->create(),y=world->create();world->find(x)->collider=Circle{{0,0},1};world->find(y)->collider=Circle{{0,0},1};
  unsigned events=0;world->update(0,{},[&](Scene&w,const CollisionEvent&e){++events;check(e.a.sceneId==x.sceneId&&e.b.sceneId==x.sceneId&&w.find(e.a)&&w.find(e.b),"event identity");});check(events==1,"pair count");
  rejects([&]{world->update(0,{},[&](Scene&w,const CollisionEvent&e){w.destroy(e.a);throw std::runtime_error("event callback");});});check(world->count()==1&&world->contacts()==0,"event exception cleanup");world->update(0);
 }
 s.clear();auto original=s.create();s.find(original)->collider=Circle{{0,0},1};bool created=false;s.update(0,[&](Scene&w,ObjectId,double){if(!created){auto n=w.create();w.find(n)->collider=Circle{{0,0},1};created=true;}});check(s.contacts()==0,"new collider updated early");s.update(0);check(s.contacts()==1,"new collider missing next update");
 std::cout<<"Scene handles, snapshots, order, stress passed\n";
}catch(const std::exception&e){std::cerr<<e.what();return 1;}}
