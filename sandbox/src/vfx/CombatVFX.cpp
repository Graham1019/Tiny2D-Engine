#include "CombatVFX.h"
#include <algorithm>
#include <cmath>
using namespace tiny2d;using namespace tiny2d_game;
namespace tiny2d_vfx {
CombatVFX::CombatVFX() {
    for(auto& id:sprites_){id=world_.create();world_.find(id)->enabled=false;}
    for(auto& id:edges_){id=ui_.create();ui_.find(id)->enabled=false;}
}
void CombatVFX::reset(){effects_={};createdSequence_=0;token_=cursor_=lastTrailTick_=dropped_=0;damage_=0;for(auto id:sprites_)world_.find(id)->enabled=false;for(auto id:edges_)ui_.find(id)->enabled=false;}
void CombatVFX::add(Effect e) {
    std::size_t slot=capacity;
    for(std::size_t i=0;i<capacity;++i) {
        const auto& candidate=effects_[i];
        if(!candidate.life){slot=i;break;}
        if(candidate.kind>e.kind)continue;
        if(slot==capacity || candidate.kind<effects_[slot].kind ||
           (candidate.kind==effects_[slot].kind && candidate.createdSequence<effects_[slot].createdSequence))slot=i;
    }
    if(slot==capacity){++dropped_;return;}
    if(effects_[slot].life)++dropped_;
    e.createdSequence=++createdSequence_;
    effects_[slot]=e;
}
void CombatVFX::consume(const GameSession& g){
    if(token_!=g.playerId().sceneId){reset();token_=g.playerId().sceneId;}
    auto end=g.eventSequence();if(end-cursor_>GameSession::eventCapacity){dropped_+=end-cursor_-GameSession::eventCapacity;cursor_=end-GameSession::eventCapacity;}
    while(cursor_<end){auto e=g.event(++cursor_);if(!e)continue;
        if(e->kind==CombatEventKind::PlayerDamaged)damage_=.12;
        else add({e->position,{},0,e->kind==CombatEventKind::EnemyKilled?.3:.15,e->kind==CombatEventKind::EnemyKilled?EffectKind::Death:EffectKind::Hit});
    }
    if(g.isPlaying()&&g.tickCount()!=lastTrailTick_){lastTrailTick_=g.tickCount();
        for(auto [id,e]:g.entities())if(e.kind==EntityKind::PlayerProjectile){auto& o=*g.scene().find(id);add({o.transform.position+Vector2{4,4},o.velocity,0,.08,EffectKind::Trail});}
    }
}
void CombatVFX::update(double dt){
    if(!std::isfinite(dt)||dt<0)throw std::invalid_argument("invalid VFX delta");
    damage_=std::max(0.0,damage_-dt);
    for(auto& e:effects_)if(e.life){e.age+=dt;if(e.age>=e.life)e.life=0;}
}
std::size_t CombatVFX::active()const {std::size_t n=0;for(auto e:effects_)n+=e.life>0;return n;}
Vector2 CombatVFX::shake()const {float a=3*damage_/.12;return {a*float(std::cos((.12-damage_)*180)),a*.5f*float(std::sin((.12-damage_)*220))};}
void CombatVFX::present(const GameSession& g,int w,int h){
    if(w<=0||h<=0)throw std::invalid_argument("VFX viewport must be positive");
    for(auto id:sprites_)world_.find(id)->enabled=false;
    auto sprite=[&](std::size_t i,Vector2 p,Vector2 size,float r,float green,float b,float a){auto& o=*world_.find(sprites_[i]);o.enabled=true;o.transform.position=p;o.sprite=SpriteData{0,size,r,green,b,a,2};};
    const Vector2 dirs[]={{1,0},{0,1},{-1,0},{0,-1}};
    for(std::size_t i=0;i<capacity;++i){auto e=effects_[i];if(!e.life)continue;float fade=1-e.age/e.life;
        for(unsigned j=0;j<((e.kind==EffectKind::Trail)?2u:4u);++j){float size=(e.kind==EffectKind::Trail)?3: ((e.kind==EffectKind::Death)?6:4)*fade+1;Vector2 p=e.p;
            if((e.kind==EffectKind::Trail)){p.x-=e.v.x*(j+1)*.012f;p.y-=e.v.y*(j+1)*.012f;}
            else {p.x+=dirs[j].x*e.age*((e.kind==EffectKind::Death)?85:55);p.y+=dirs[j].y*e.age*((e.kind==EffectKind::Death)?85:55);}
            sprite(i*4+j,{p.x-size/2,p.y-size/2},{size,size},1,(e.kind==EffectKind::Death)?.28f:.85f,.1f,fade*((e.kind==EffectKind::Trail)?.45f:1));
            world_.find(sprites_[i*4+j])->sprite->renderOrder=2+static_cast<int>(e.kind);
        }
    }
    std::size_t marker=capacity*4;
    for(auto [id,e]:g.entities())if(e.kind!=EntityKind::PlayerProjectile&&marker<sprites_.size()){
        auto p=g.scene().find(id)->transform.position;
        sprite(marker++,p+Vector2{8,8},{8,8},e.kind==EntityKind::Player?.65f:.5f,e.kind==EntityKind::Player?.9f:.08f,e.kind==EntityKind::Player?1:.04f,1);
        world_.find(sprites_[marker-1])->sprite->renderOrder=1;
    }
    for(unsigned i=0;i<4;++i){auto& o=*ui_.find(edges_[i]);o.enabled=damage_>0&&w>0&&h>0;
        o.transform.position={i==1?float(w-4):0,i==3?float(h-4):0};
        o.sprite=SpriteData{0,{i<2?4.f:float(w),i<2?float(h):4.f},1,.06f,.04f,float(.7*damage_/.12),3};}
}
}
