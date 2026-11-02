#include "GameSessionTestAccess.h"
#include "tiny2d/FixedTimestep.h"
#include <limits>
using namespace tiny2d_game;
using A=GameSessionTestAccess;
int main(){try {
    auto c=quiet();GameSession g(c);check(g.playerHealth()==5,"initial hp");
    A::enemy(g,{0,0});A::enemy(g,{0,0});g.fixedUpdate();check(g.playerHealth()==4,"one contact per tick");
    step(g,35);check(g.playerHealth()==4,"invulnerable through tick 36");
    g.fixedUpdate();check(g.playerHealth()==3,"vulnerable at tick 37");
    step(g,108);check(!g.playerAlive()&&g.playerHealth()==0,"death clamps");
    auto p=A::center(g,g.playerId());g.fixedUpdate({1,1});step(g,100);
    check(A::center(g,g.playerId()).x==p.x&&g.firedCount()==0,"dead movement/fire");
    GameSession moving(c);moving.fixedUpdate({1,1});auto q=A::center(moving,moving.playerId());
    check(std::abs(std::hypot(q.x,q.y)-3)<1e-5,"normalized diagonal");
    for(float v:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
        auto t=moving.tickCount();rejects([&]{moving.fixedUpdate({v,0});});check(moving.tickCount()==t,"input rejected before mutation");
    }
    for(int field=0;field<8;++field) {auto bad=c;
        if(field==0)bad.playerMaxHealth=0;if(field==1)bad.enemyHealth=-1;
        if(field==2)bad.projectileSpeed=std::numeric_limits<float>::infinity();
        if(field==3)bad.invulnerability=std::numeric_limits<double>::quiet_NaN();
        if(field==4)bad.fireInterval=0;if(field==5)bad.maxEnemies=100000;
        if(field==6)bad.initialEnemies=bad.maxEnemies+1;if(field==7)bad.minimumSpawnInterval=86401;
        rejects([&]{GameSession invalid(bad);});
    }
    c.fireInterval=.6; c.projectileSpeed=0;c.projectileLifetime=86400;
    GameSession empty(c);step(empty,360);check(!empty.firedCount(),"no target");
    A::enemy(empty,{100,0});step(empty,35);check(!empty.firedCount(),"no backlog");
    empty.fixedUpdate();check(empty.firedCount()==1,"next opportunity");
    GameSession aim(c);auto left=A::enemy(aim,{-100,0});A::enemy(aim,{100,0});A::fire(aim);
    // Use nonzero speed to observe aim and stable tie, then nearest.
    c.projectileSpeed=420;GameSession target(c);A::enemy(target,{-100,0});auto near=A::enemy(target,{100,0});A::fire(target);
    for(auto [id,s]:target.entities())if(s.kind==EntityKind::PlayerProjectile)check(target.scene().find(id)->velocity.x==-420,"tie uses id");
    A::erase(target,near);auto closer=A::enemy(target,{0,40});A::fire(target);
    auto last=target.entities().rbegin()->first;check(target.scene().find(last)->velocity.y==420,"nearest target");
    A::erase(target,closer);(void)left;
    uint64_t shots=0;
    for(int fps:{30,60,120}) {GameSession sim(c);A::enemy(sim,{10000,0});tiny2d::FixedTimestep clock;
        for(int frame=0;frame<fps*6;++frame)for(unsigned n=clock.advance(1.0/fps);n;--n)sim.fixedUpdate();
        check(sim.tickCount()==360&&sim.firedCount()==10,"fps partition");shots=sim.firedCount();
    }
    auto deathConfig=quiet();deathConfig.playerMaxHealth=1;deathConfig.enemyHealth=10000;
    deathConfig.projectileSpeed=0;deathConfig.fireInterval=1.0/60;
    GameSession death(deathConfig);A::enemy(death,{0,0});death.fixedUpdate();
    check(!death.playerAlive()&&death.firedCount()==1,"death on firing tick");
    step(death,300);check(death.firedCount()==1&&death.playerHealth()==0,"dead player stops scheduled fire");
    check(shots==10,"shots");std::cout<<"combat passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
