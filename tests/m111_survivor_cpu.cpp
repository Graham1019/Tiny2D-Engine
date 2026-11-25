#include "GameSessionTestAccess.h"
#include "SurvivorTrace.h"
#include "tiny2d_game/SurvivorFlowController.h"
#include "ui/GameHUD.h"
#include "vfx/CombatVFX.h"

using namespace tiny2d_game;

int main() {
    try {
        for (unsigned level:{3,5}) {
          for (bool moving:{false,true}) {
            SurvivorFlowController reference(difficultyLevel(level)), observed(difficultyLevel(level));
            reference.start();
            observed.start();
            tiny2d_vfx::CombatVFX vfx;
            tiny2d_ui::GameHUD hud;
            unsigned ticks=0;
            std::array<bool,3> kinds{};
            while (reference.game().isPlaying()) {
                auto sequence=reference.game().eventSequence();
                GameplayInput input;
                if (moving && ticks%80 < 55) {
                    input.moveX=(ticks/31)%2 ? -1.f : 1.f;
                    input.moveY=(ticks/19)%2 ? 1.f : -1.f;
                }
                reference.fixedUpdate(input);
                observed.fixedUpdate(input);
                auto& game=observed.game();
                vfx.update(1./60);
                vfx.consume(game);
                vfx.present(game,320,240);
                hud.update(game,320,240,observed.difficulty());
                check(survivorTraceRow(reference.game(),sequence)==survivorTraceRow(game,sequence),
                      "per-tick entity state, timers or CombatEvents changed during preparation");
                for (auto i=sequence+1;i<=game.eventSequence();++i)
                    kinds.at(static_cast<unsigned>(game.event(i)->kind))=true;
                ++ticks;
            }
            if (!moving) check(kinds[0]&&kinds[1]&&kinds[2],"all combat event kinds observed");
            const auto terminal=survivorTraceRow(reference.game(),reference.game().eventSequence());
            observed.fixedUpdate({1,1});
            check(terminal==survivorTraceRow(observed.game(),observed.game().eventSequence()),"terminal freeze");
            MenuInput restart;
            restart.restart=true;
            check(observed.input(restart)&&observed.game().tickCount()==0,"terminal restart");
            vfx.consume(observed.game());
            check(vfx.active()==0,"old effects retired after restart");
            std::cout<<"level="<<level<<" moving="<<moving<<" compared_ticks="<<ticks<<" combat_events="<<reference.game().eventSequence()<<'\n';
          }
        }
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
