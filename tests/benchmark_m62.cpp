#include "GameSessionTestAccess.h"
#include "tiny2d_game/SessionController.h"
#include "ui/GameHUD.h"
#include "tiny2d/SceneRenderer.h"
#include "tiny2d/Window.h"
#include <chrono>
using namespace tiny2d;using namespace tiny2d_game;
using Clock=std::chrono::steady_clock;
static double us(Clock::time_point a,Clock::time_point b){return std::chrono::duration<double,std::micro>(b-a).count();}
int main(){
    Window window(800,600,"M62 measurements");Camera2D world(800,600),ui(800,600);ui.setPosition(400,300);
    SceneRenderer renderer;renderer.setAsset(0,std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}}));
    std::cout<<"workload,sample,fixed_us,hud_us,world_us,ui_us,total_submit_us,world_draws,ui_draws,world_objects,ui_objects\n";
    for(int load=0;load<5;++load){auto c=quiet();c.playerMaxHealth=load==3?1:5;SessionController session(c,{load==2?1ull:5400ull});auto& g=session.game();
        if(load==1){for(int i=0;i<48;++i)GameSessionTestAccess::enemy(g,{float(100+i%12*32),float(100+i/12*32)});for(int i=0;i<64;++i)GameSessionTestAccess::shot(g,{float(100+i*16),-100},5000);}
        if(load==2)g.fixedUpdate();if(load==3){GameSessionTestAccess::enemy(g,{0,0});g.fixedUpdate();}
        if(load==4){step(g,5400);session.restartKey(true);}
        tiny2d_ui::GameHUD hud;
        for(int i=-30;i<300;++i){auto& game=session.game();auto a=Clock::now();game.fixedUpdate();auto b=Clock::now();hud.update(game,800,600);auto d=Clock::now();window.clear(0,0,0);auto e=Clock::now();renderer.render(game.scene(),world);auto f=Clock::now();auto wd=renderer.stats().drawCalls;renderer.render(hud.scene(),ui);auto h=Clock::now();
            if(i>=0)std::cout<<load<<','<<i<<','<<us(a,b)<<','<<us(b,d)<<','<<us(e,f)<<','<<us(f,h)<<','<<us(e,h)<<','<<wd<<','<<renderer.stats().drawCalls<<','<<game.scene().count()<<','<<hud.scene().count()<<'\n';}
    }
}
