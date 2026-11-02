#include "GameSessionTestAccess.h"
using namespace tiny2d_game;
using A=GameSessionTestAccess;
int main(){try {
    auto c=quiet();GameSession g(c);auto e=A::enemy(g,{50,0});auto p=A::shot(g,{50,0});
    g.fixedUpdate();check(g.hitCount()==1&&g.entities().at(e).health==1&&!g.scene().find(p),"single damage and invalidation");
    auto second=A::enemy(g,{50,0});A::shot(g,{50,0});A::shot(g,{50,0});g.fixedUpdate();
    check(g.killCount()==1&&g.hitCount()==3&&!g.scene().find(e)&&g.entities().at(second).health==1,"multi shot skips dead target");
    GameSession multi(c);auto a=A::enemy(multi,{50,0});auto b=A::enemy(multi,{50,0});A::shot(multi,{50,0});multi.fixedUpdate();
    check(multi.hitCount()==1&&multi.entities().at(a).health==1&&multi.entities().at(b).health==2,"one target per shot");
    GameSession life(c);auto expired=A::shot(life,{0,0},2);A::shot(life,{0,0},2);
    life.fixedUpdate();check(life.projectileCount()==2&&life.playerHealth()==5&&!life.hitCount(),"ignored projectile pairs");
    auto enemy=A::enemy(life,{0,0});life.fixedUpdate();check(!life.projectileCount()&&!life.scene().find(expired)&&life.entities().at(enemy).health==2,"expiry before collisions");
    c.maxProjectiles=1;c.projectileSpeed=0;c.fireInterval=1.0/60;c.projectileLifetime=.1;
    GameSession cap(c);A::enemy(cap,{100,0});step(cap,5);check(cap.firedCount()==1&&cap.projectileCount()==1,"capacity skips opportunities");
    step(cap,2);check(cap.firedCount()==2,"capacity recovers without backlog");
    c=quiet();GameSession overlap(c);A::enemy(overlap,{0,0});A::fire(overlap);
    for(auto [id,s]:overlap.entities())if(s.kind==EntityKind::PlayerProjectile)check(overlap.scene().find(id)->velocity.x==420,"overlap fallback");
    A::erase(life,enemy); // Keep expiry stress Playing; terminal sessions now freeze.
    for(int i=0;i<1200;++i) {
        auto old=A::shot(life,{1000,0},1);life.fixedUpdate();
        check(!life.scene().find(old),"stress stale");auto fresh=A::shot(life,{1000,0},1);
        check(old.index==fresh.index&&old.generation!=fresh.generation,"generation reuse");
        life.fixedUpdate();check(!life.projectileCount(),"stress bounded");
    }
    GameSession kills(quiet());
    for(int i=0;i<1200;++i) {
        auto victim=A::enemy(kills,{80,0});A::shot(kills,{80,0});A::shot(kills,{80,0});kills.fixedUpdate();
        check(!kills.scene().find(victim)&&kills.enemyCount()==0&&kills.projectileCount()==0,"simultaneous cleanup stress");
        check(kills.killCount()==uint64_t(i+1),"exact stress kill count");
    }
    std::cout<<"projectile lifecycle passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
