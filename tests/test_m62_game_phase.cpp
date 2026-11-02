#include "GameSessionTestAccess.h"
#include <limits>
#include <sstream>
using namespace tiny2d_game;
static std::string snapshot(const GameSession& g){
    std::ostringstream s;s<<g.tickCount()<<','<<g.playerHealth()<<','<<g.killCount()<<','<<g.spawnedCount()<<','<<g.firedCount()<<','<<g.hitCount()<<','<<g.scene().contacts();
    for(auto [id,e]:g.entities()){auto& o=*g.scene().find(id);s<<id.sceneId<<','<<id.index<<','<<id.generation<<','<<e.health<<','<<e.expires<<','<<o.transform.position.x<<','<<o.transform.position.y<<','<<o.velocity.x<<','<<o.velocity.y<<','<<o.sprite->r<<','<<o.sprite->g<<','<<o.sprite->b;}
    return s.str();
}
int main(){try {
    rejects([]{GameSession g({}, {0});});rejects([]{GameSession g({}, {UINT64_MAX});});
    GameSession g(quiet());check(g.isPlaying()&&g.tickCount()==0&&g.remainingTicks()==5400,"initial");
    step(g,5399);check(g.isPlaying()&&g.remainingTicks()==1,"before victory");g.fixedUpdate();
    check(g.phase()==GamePhase::Victory&&g.remainingSeconds()==0,"victory boundary");
    auto count=g.scene().count();step(g,1000);g.fixedUpdate({std::numeric_limits<float>::quiet_NaN(),0});
    check(g.tickCount()==5400&&g.scene().count()==count&&g.firedCount()==0,"terminal no-op ignores input");
    auto c=quiet();c.playerMaxHealth=1;GameSession death(c,{1});GameSessionTestAccess::enemy(death,{0,0});
    death.fixedUpdate();check(death.phase()==GamePhase::GameOver,"death wins same tick");
    auto p=death.scene().find(death.playerId())->transform.position;auto n=death.scene().count();
    step(death,1000);check(death.tickCount()==1&&death.playerHealth()==0&&death.scene().count()==n&&death.scene().find(death.playerId())->transform.position.x==p.x,"death frozen");
    GameSession one(quiet(),{1});one.fixedUpdate();check(one.phase()==GamePhase::Victory,"one tick");
    GameSession early(c);GameSessionTestAccess::enemy(early,{0,0});early.fixedUpdate();check(early.remainingTicks()==5399,"early death timer");
    GameSession a,b;for(int i=0;i<6400;++i){GameplayInput in{float((i/120)%3-1),float((i/180)%3-1)};a.fixedUpdate(in);b.fixedUpdate(in);check(a.tickCount()==b.tickCount()&&a.killCount()==b.killCount()&&a.playerHealth()==b.playerHealth(),"replay");}
    GameSession terminal;step(terminal,5400);check(!terminal.isPlaying(),"default terminal");auto frozen=snapshot(terminal);step(terminal,1000);check(snapshot(terminal)==frozen,"complete combat and scene freeze");
    std::cout<<"M62 phase passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
