#include "M5Test.h"
#include "tiny2d/Scene.h"
#include <type_traits>
static_assert(!std::is_copy_assignable_v<tiny2d::GameObject>);
using namespace tiny2d;
int main(){try{
 Scene s;auto a=s.create();check(s.find(a)&&s.find(a)->id()==a&&s.count()==1,"create");check(s.destroy(a)&&!s.destroy(a)&&!s.find(a),"destroy");auto b=s.create();check(a.index==b.index&&a.generation!=b.generation,"generation");check(!s.find({}),"invalid");
 s.find(b)->enabled=false;unsigned calls=0;s.update(1,[&](Scene&,ObjectId,double){++calls;});check(!calls,"disabled");s.find(b)->enabled=true;
 ObjectId c;s.update(0,[&](Scene&world,ObjectId id,double){c=world.create();world.destroy(id);++calls;});check(calls==1&&s.count()==1&&!s.find(b),"mutation snapshot");
 auto d=s.create();s.find(c)->sprite=SpriteData{};s.find(d)->sprite=SpriteData{};check(s.renderables()==std::vector<ObjectId>({c,d}),"stable render order");
 s.clear();check(!s.find(c)&&!s.find(d)&&s.count()==0,"clear");
 for(int round=0;round<20;++round){std::vector<ObjectId> ids;for(int i=0;i<1000;++i)ids.push_back(s.create());auto pointer=s.find(ids.front());s.create();check(pointer==s.find(ids.front()),"stable storage");s.clear();for(auto id:ids)check(!s.find(id),"stress stale");}
 auto id=s.create();rejects([&]{s.update(0,[](Scene&,ObjectId,double){throw std::runtime_error("callback");});});check(s.destroy(id),"exception recovery");
 std::cout<<"Scene handles, snapshots, order, stress passed\n";
}catch(const std::exception&e){std::cerr<<e.what();return 1;}}
