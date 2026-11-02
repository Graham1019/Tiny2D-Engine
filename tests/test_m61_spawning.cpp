#include "GameSessionTestAccess.h"
#include <algorithm>
using namespace tiny2d_game;
using A=GameSessionTestAccess;
int main(){try {
    GameSession defaults;check(defaults.enemyCount()==8,"initial enemies");
    for(auto [id,s]:defaults.entities())if(s.kind==EntityKind::Enemy) {
        auto p=A::center(defaults,id);check(std::hypot(p.x,p.y)>=319,"safe initial radius");check(s.health==2,"enemy health");
    }
    auto c=quiet();c.enemySpeed=65;GameSession ai(c);auto enemy=A::enemy(ai,{100,0});ai.fixedUpdate();
    check(std::abs(ai.scene().find(enemy)->velocity.x+65)<1e-5,"chase speed");
    c=quiet();c.initialSpawnInterval=1.5;c.minimumSpawnInterval=.6;c.maxEnemies=2;
    GameSession schedule(c);step(schedule,89);check(!schedule.enemyCount(),"spawn boundary before");
    schedule.fixedUpdate();check(schedule.enemyCount()==1,"spawn at 90");step(schedule,1000);
    check(schedule.enemyCount()==2&&schedule.spawnedCount()==2,"enemy cap");
    GameSession a,b;
    for(int i=0;i<6000;++i) {
        GameplayInput input{float((i/120)%3-1),float((i/180)%3-1)};a.fixedUpdate(input);b.fixedUpdate(input);
        check(a.playerHealth()==b.playerHealth()&&a.killCount()==b.killCount()&&a.spawnedCount()==b.spawnedCount()&&
            a.firedCount()==b.firedCount()&&a.hitCount()==b.hitCount()&&a.scene().count()==b.scene().count(),"repeat stats");
        auto ia=a.entities().begin(),ib=b.entities().begin();
        for(;ia!=a.entities().end();++ia,++ib) {
            check(ia->second.serial==ib->second.serial&&ia->second.health==ib->second.health,"logical identity");
            auto pa=a.scene().find(ia->first)->transform.position,pb=b.scene().find(ib->first)->transform.position;
            check(pa.x==pb.x&&pa.y==pb.y&&!a.scene().find(ib->first),"repeat positions and foreign handles");
        }
        check(a.enemyCount()<=48&&a.projectileCount()<=64,"bounded long run");
    }
    std::cout<<"spawn schedule, AI and 6000-tick determinism passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
