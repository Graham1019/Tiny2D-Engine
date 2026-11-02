#include "GameSessionTestAccess.h"
#include "tiny2d_game/SessionController.h"
#include <cstdlib>
#include <new>
static bool failAllocation=false;
void* operator new(std::size_t n){if(failAllocation)throw std::bad_alloc();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}
using namespace tiny2d_game;
int main(){try {
    SessionController s(quiet(),{1});check(!s.restartKey(true),"playing cannot restart");s.game().fixedUpdate();
    check(!s.restartKey(true),"held from playing");s.restartKey(false);
    auto old=s.game().playerId();failAllocation=true;bool failed=false;
    try{s.restartKey(true);}catch(const std::bad_alloc&){failed=true;}failAllocation=false;
    check(failed&&s.game().playerId()==old&&s.game().phase()==GamePhase::Victory,"failed replacement retains session");s.restartKey(false);
    for(int i=0;i<150;++i){old=s.game().playerId();check(s.restartKey(true),"restart");
        auto& g=s.game();check(g.isPlaying()&&g.playerHealth()==5&&g.tickCount()==0&&g.killCount()==0&&g.firedCount()==0&&g.hitCount()==0&&g.spawnedCount()==0&&g.projectileCount()==0&&!g.invulnerable(),"reset counters");
        check(g.playerId().sceneId!=old.sceneId&&!g.scene().find(old)&&g.scene().count()==1,"fresh scene and containers");
        g.fixedUpdate();check(!s.restartKey(true),"hold no repeat");s.restartKey(false);
    }
    auto c=quiet();c.playerMaxHealth=1;SessionController d(c);
    GameSessionTestAccess::enemy(d.game(),{0,0});d.game().fixedUpdate();check(d.game().phase()==GamePhase::GameOver&&d.restartKey(true)&&d.game().playerHealth()==1,"death restart");
    SessionController defaults;step(defaults.game(),5400);check(defaults.restartKey(true),"default restart");
    check(defaults.game().enemyCount()==8&&defaults.game().spawnedCount()==8&&defaults.game().remainingTicks()==5400&&defaults.game().firedCount()==0&&defaults.game().killCount()==0,"default reset");
    step(defaults.game(),35);check(defaults.game().firedCount()==0,"fire cooldown reset");defaults.game().fixedUpdate();check(defaults.game().firedCount()==1,"first fire reset");
    std::cout<<"150 restarts and allocation failure passed\n";
}catch(const std::exception&e){failAllocation=false;std::cerr<<e.what()<<'\n';return 1;}}
