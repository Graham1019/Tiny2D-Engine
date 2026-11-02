#include "GameSessionTestAccess.h"
#include "tiny2d_game/SurvivorFlowController.h"
#include <cstdlib>
#include <new>
static bool fail=false;
void* operator new(std::size_t n){if(fail)throw std::bad_alloc();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}
using namespace tiny2d_game;
int main(){try{
    SurvivorFlowController menu;check(menu.phase()==SurvivorPhase::DifficultySelection&&menu.difficulty()==DifficultyLevel::Normal,"initial menu");
    for(int i=0;i<100;++i)menu.fixedUpdate();rejects([&]{menu.game();});
    MenuInput lost;lost.focused=false;menu.input(lost);lost.focused=true;lost.enter=true;check(!menu.input(lost),"focus enter blocked");menu.input({});
    fail=true;bool failed=false;try{menu.input(lost);}catch(const std::bad_alloc&){failed=true;}fail=false;check(failed&&menu.phase()==SurvivorPhase::DifficultySelection,"start failure");
    for(unsigned d=1;d<=5;++d){SurvivorFlowController f;MenuInput in;in.levels[d-1]=true;f.input(in);check(unsigned(f.difficulty())==d,"selection");f.input(in);in.enter=true;check(f.input(in),"start edge");auto old=f.game().playerId();check(!f.input(in)&&f.game().playerId()==old,"enter held");
        in.levels.fill(true);f.input(in);check(unsigned(f.difficulty())==d,"locked level");
        for(int repeat=0;repeat<5;++repeat){step(f.game(),5400);old=f.game().playerId();in.restart=false;f.input(in);
            if(repeat==0){fail=true;failed=false;try{in.restart=true;f.input(in);}catch(const std::bad_alloc&){failed=true;}fail=false;check(failed&&f.game().playerId()==old&&!f.game().isPlaying(),"restart strong guarantee");in.restart=false;f.input(in);}
            in.restart=true;check(f.input(in),"restart");check(!f.input(in),"held R");check(f.game().tickCount()==0&&f.game().playerHealth()==5&&f.game().killCount()==0&&f.game().spawnedCount()==8&&f.game().firedCount()==0&&unsigned(f.difficulty())==d&&!f.game().scene().find(old),"fresh same level");
            MenuInput blur;blur.focused=false;f.input(blur);blur.focused=true;blur.enter=true;blur.restart=true;check(!f.input(blur),"held across focus restore");f.input({});
            step(f.game(),combatTicks(makeDifficultyConfig(f.difficulty()).fireInterval));check(f.game().firedCount()==1,"difficulty retained");
        }
    }
}catch(const std::exception&e){fail=false;std::cerr<<e.what();return 1;}}
