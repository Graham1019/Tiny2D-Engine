#include "GameSessionTestAccess.h"
#include "tiny2d_game/Difficulty.h"
#include "tiny2d/FixedTimestep.h"
using namespace tiny2d_game;using A=GameSessionTestAccess;
static void same(const GameSession&a,const GameSession&b){
 check(a.tickCount()==b.tickCount()&&a.playerHealth()==b.playerHealth()&&a.killCount()==b.killCount()&&a.firedCount()==b.firedCount()&&a.spawnedCount()==b.spawnedCount()&&a.hitCount()==b.hitCount()&&a.entities().size()==b.entities().size(),"deterministic counters");
 auto j=b.entities().begin();for(auto [id,e]:a.entities()){auto p=a.scene().find(id)->transform.position,q=b.scene().find(j->first)->transform.position;check(e.serial==j->second.serial&&e.health==j->second.health&&p.x==q.x&&p.y==q.y,"deterministic positions");++j;}
}
int main(){try{for(unsigned d=1;d<=5;++d){auto c=makeDifficultyConfig(difficultyLevel(d));GameSession a(c),b(c),stationary(c);
 a.fixedUpdate();for(auto [id,e]:a.entities())if(e.kind==EntityKind::Enemy){auto v=a.scene().find(id)->velocity;check(std::abs(std::hypot(v.x,v.y)-c.enemySpeed)<.001,"enemy speed");}b.fixedUpdate();
 for(int i=1;i<6400;++i){GameplayInput in{float((i/120)%3-1),float((i/180)%3-1)};a.fixedUpdate(in);b.fixedUpdate(in);stationary.fixedUpdate();same(a,b);check(a.enemyCount()<=48&&a.projectileCount()<=64,"bounded");}check(stationary.phase()==GamePhase::GameOver,"early death");
 auto fixture=c;fixture.enemySpeed=0;fixture.initialEnemies=0;fixture.initialSpawnInterval=fixture.minimumSpawnInterval=86400;
 GameSession shots(fixture);A::enemy(shots,{100000,0});for(unsigned i=1;i<=600;++i){shots.fixedUpdate();check(shots.firedCount()==i/combatTicks(c.fireInterval),"actual fire boundaries");for(auto [id,e]:shots.entities())if(e.kind==EntityKind::PlayerProjectile){auto v=shots.scene().find(id)->velocity;check(std::abs(std::hypot(v.x,v.y)-c.projectileSpeed)<.001,"projectile speed");}}
 fixture=c;fixture.initialEnemies=0;fixture.enemySpeed=0;fixture.fireInterval=86400;GameSession spawn(fixture);uint64_t deadline=combatTicks(c.initialSpawnInterval),count=0;
 for(unsigned t=1;t<=5400;++t){spawn.fixedUpdate();if(t==deadline){++count;deadline=t+combatTicks(c.initialSpawnInterval+(c.minimumSpawnInterval-c.initialSpawnInterval)*t/5400.0);}check(spawn.spawnedCount()==count,"actual ramp opportunities");std::vector<tiny2d::ObjectId> ids;for(auto [id,e]:spawn.entities())if(e.kind==EntityKind::Enemy)ids.push_back(id);for(auto id:ids)A::erase(spawn,id);}
 check(spawn.phase()==GamePhase::Victory,"tick 5400 victory");auto frozen=spawn.spawnedCount();step(spawn,1000);check(spawn.tickCount()==5400&&spawn.spawnedCount()==frozen,"terminal freeze");
 GameSession reference(c);for(int i=0;i<600;++i)reference.fixedUpdate({1,0});for(int fps:{30,60,120}){GameSession g(c);tiny2d::FixedTimestep clock;for(int i=0;i<fps*10;++i)for(unsigned n=clock.advance(1.0/fps);n;--n)g.fixedUpdate({1,0});same(reference,g);}
 fixture=c;fixture.initialEnemies=0;fixture.enemySpeed=0;fixture.fireInterval=fixture.initialSpawnInterval=fixture.minimumSpawnInterval=86400;GameSession damage(fixture);A::enemy(damage,{0,0});damage.fixedUpdate();step(damage,35);check(damage.playerHealth()==4,"36 tick immunity");damage.fixedUpdate();check(damage.playerHealth()==3,"immunity boundary");
 GameSession hits(fixture);auto e=A::enemy(hits,{100,0});A::enemy(hits,{100,0});auto p=A::shot(hits,{100,0});hits.fixedUpdate();check(hits.hitCount()==1&&!hits.scene().find(p),"single hit");A::shot(hits,{100,0});hits.fixedUpdate();check(hits.killCount()==1&&!hits.scene().find(e),"single score");hits.fixedUpdate();check(hits.killCount()==1,"no duplicate score");
 fixture.playerMaxHealth=1;GameSession tie(fixture);step(tie,5399);A::enemy(tie,{0,0});tie.fixedUpdate();check(tie.phase()==GamePhase::GameOver,"death priority at 5400");
 }}catch(const std::exception&e){std::cerr<<e.what();return 1;}}
