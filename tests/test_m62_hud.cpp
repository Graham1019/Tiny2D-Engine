#include "GameSessionTestAccess.h"
#include "ui/GameHUD.h"
#include "ui/BitmapFont.h"
using namespace tiny2d_game;
using namespace tiny2d_ui;
int main(){try {
    check(formatTime(90)=="01:30"&&formatTime(89)=="01:29"&&formatTime(0)=="00:00"&&formatTime(600)=="10:00","time format");
    GameSession g(quiet());GameHUD hud;hud.update(g,800,600);auto r=hud.revision();g.fixedUpdate();hud.update(g,800,600);check(hud.revision()==r,"tick 1 cached");
    step(g,59);hud.update(g,800,600);check(hud.revision()==r+1,"tick 60 updates");
    GameSessionTestAccess::enemy(g,{0,0});g.fixedUpdate();step(g,36);check(g.playerHealth()==3,"real hp3");
    for(int w:{320,800}){hud.update(g,w,w==320?240:600);for(auto id:hud.scene().objects()){auto&o=*hud.scene().find(id);check(o.transform.position.x>=0&&o.transform.position.y>=0&&o.transform.position.x+o.sprite->size.x<=w,"bounded layout");}}
    step(g,108);check(g.playerHealth()==0,"hp zero");hud.update(g,320,240);r=hud.revision();hud.update(g,320,240);check(r==hud.revision(),"terminal cache");
    auto c=quiet();c.enemyHealth=1;GameSession kills(c);for(int i=0;i<100;++i){GameSessionTestAccess::enemy(kills,{100,0});GameSessionTestAccess::shot(kills,{100,0});kills.fixedUpdate();if(i==9||i==99)hud.update(kills,320,240);}check(kills.killCount()==100,"real kills");
    GameSession win(quiet(),{1});win.fixedUpdate();hud.update(win,320,240);check(hud.scene().count()>200,"overlay glyphs");
    std::cout<<"HUD layout cache and data passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
