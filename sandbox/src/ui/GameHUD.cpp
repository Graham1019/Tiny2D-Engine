#include "GameHUD.h"
#include "BitmapFont.h"
#include <algorithm>
namespace tiny2d_ui {
std::string formatTime(uint64_t seconds) {
    auto pad=[](uint64_t n){return (n<10?"0":"")+std::to_string(n);};
    return pad(seconds/60)+":"+pad(seconds%60);
}
void GameHUD::update(const tiny2d_game::GameSession& game,int width,int height,tiny2d_game::DifficultyLevel level) {
    if(width<=0||height<=0)return;
    if(sceneToken_!=game.playerId().sceneId){sceneToken_=game.playerId().sceneId;lastKills_=game.killCount();flashUntil_=0;}
    if(lastKills_!=game.killCount()){lastKills_=game.killCount();flashUntil_=game.tickCount()+12;}
    bool killFlash=game.isPlaying()&&game.tickCount()<flashUntil_;
    auto hp="HP "+std::to_string(game.playerHealth())+"/"+std::to_string(game.playerMaxHealth());
    auto kills="KILLS "+std::to_string(game.killCount());
    auto time=formatTime((game.remainingTicks()+59)/60);
    auto survived="SURVIVED "+formatTime(game.tickCount()/60);
    // Kill counter flashes for a bounded number of simulation ticks.
    auto difficulty="LV "+std::to_string(unsigned(level))+" "+tiny2d_game::difficultyName(level);
    auto key=difficulty+hp+kills+time+std::to_string(int(game.phase()))+std::to_string(game.invulnerable())+std::to_string(killFlash)+
        ":"+std::to_string(width)+":"+std::to_string(height)+(game.isPlaying()?"":survived);
    if(key==key_)return;
    auto next=std::make_unique<tiny2d::Scene>();auto& s=*next;
    float scale=std::min({2.f,width/320.f,height/240.f});
    float margin=10*scale;
    rectangle(s,0,0,width,34*scale,.025f,.035f,.055f,.94f);
    text(s,hp,margin,6*scale,scale,1,game.invulnerable()?.35f:1,game.invulnerable()?.35f:1);
    rectangle(s,margin,18*scale,65*scale,6*scale,.25f,.15f,.18f);
    rectangle(s,margin,18*scale,65*scale*game.playerHealth()/game.playerMaxHealth(),6*scale,.2f,.85f,.4f);
    text(s,time,(width-textWidth(time,scale))/2,10*scale,scale);
    text(s,kills,width-margin-textWidth(kills,scale),10*scale,scale,1,killFlash?1:.85f,killFlash?1:.3f);
    text(s,difficulty,margin,height-12*scale,scale);
    if(!game.isPlaying()) {
        float panel=std::min(width-2*margin,280*scale), top=height/2.f-50*scale;
        rectangle(s,(width-panel)/2,top,panel,104*scale,.025f,.035f,.06f,.92f);
        auto centered=[&](const std::string& t,float y,float z){text(s,t,(width-textWidth(t,z))/2,y,z);};
        centered(game.phase()==tiny2d_game::GamePhase::Victory?"VICTORY":"GAME OVER",top+10*scale,2*scale);
        centered(survived,top+36*scale,scale);centered(kills,top+54*scale,scale);
        centered("PRESS R TO RESTART",top+80*scale,scale);
    }
    scene_.swap(next);key_.swap(key);++revision_;
}
}
