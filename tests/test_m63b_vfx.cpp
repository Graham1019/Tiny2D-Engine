#include "GameSessionTestAccess.h"
#include "tiny2d_game/SessionController.h"
#include "vfx/CombatVFX.h"
using namespace tiny2d_game;using A=GameSessionTestAccess;
int main(){try{tiny2d_vfx::CombatVFX v;GameSession a,b;for(int i=0;i<30000;++i){GameplayInput in{1,float((i/100)%3-1)};a.fixedUpdate(in);b.fixedUpdate(in);v.update(1./60);v.consume(a);v.present(a,320,240);check(a.playerHealth()==b.playerHealth()&&a.killCount()==b.killCount()&&a.hitCount()==b.hitCount()&&a.firedCount()==b.firedCount()&&a.spawnedCount()==b.spawnedCount(),"VFX independent");auto j=b.entities().begin();for(auto [id,e]:a.entities()){auto p=a.scene().find(id)->transform.position,q=b.scene().find(j->first)->transform.position;check(p.x==q.x&&p.y==q.y,"positions identical");++j;}check(v.active()<=128&&v.scene().count()==v.spriteBudget,"bounded slots");}
 GameSession stress(quiet(),{50000});for(int i=0;i<30000;++i){A::enemy(stress,{80,0});A::shot(stress,{80,0});A::shot(stress,{80,0});stress.fixedUpdate();v.consume(stress);v.update(1./600);v.present(stress,800,600);check(v.active()<=128,"bounded burst");}check(v.dropped()>0,"overflow observed");v.update(1);v.present(stress,320,240);check(v.active()==0,"expiry");for(auto id:v.scene().objects())check(!v.scene().find(id)->collider,"no collision");
 SessionController s(quiet(),{1});for(int i=0;i<120;++i){A::enemy(s.game(),{0,0});s.game().fixedUpdate();v.consume(s.game());check(v.shake().x!=0,"damage feedback");s.restartKey(false);s.restartKey(true);v.consume(s.game());v.present(s.game(),320,240);check(!v.active()&&v.shake().x==0&&v.overlay().renderables().empty(),"restart cleans");}
 rejects([&]{v.update(-1);});
}catch(const std::exception&e){std::cerr<<e.what();return 1;}}
