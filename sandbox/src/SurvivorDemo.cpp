#include "SurvivorDemo.h"
#include "tiny2d/Application.h"
#include "tiny2d/SceneRenderer.h"
#include "tiny2d/FixedTimestep.h"
#include "tiny2d_game/SurvivorFlowController.h"
#include "ui/GameHUD.h"
#include "ui/DifficultyMenu.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
using namespace tiny2d;
namespace {
class Survivor final:public Application {
public:
    explicit Survivor(unsigned limit,unsigned level):Application(800,600,"Tiny2D Survivor"),session_(tiny2d_game::difficultyLevel(level)),limit_(limit) {
        if(limit_)session_.start();
        renderer_.setAsset(0,std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}}));
        for(int i=-20;i<=20;++i) {
            auto id=grid_.create();auto& o=*grid_.find(id);
            o.transform.position={float(i*80),-1600};o.sprite=SpriteData{0,{1,3200},.12f,.17f,.22f,1,-1};
            id=grid_.create();auto& h=*grid_.find(id);
            h.transform.position={-1600,float(i*80)};h.sprite=SpriteData{0,{3200,1},.12f,.17f,.22f,1,-1};
        }
    }
private:
    void update(float dt) override {
        if(input().isKeyPressed(Key::Escape))window().requestClose();
        tiny2d_game::MenuInput menuInput;
        for(unsigned i=0;i<5;++i)menuInput.levels[i]=input().isKeyPressed('1'+i);
        menuInput.enter=input().isKeyPressed(257);menuInput.restart=input().isKeyPressed('R');menuInput.focused=input().focused();
        bool restarted=limit_?false:session_.input(menuInput);
        if(session_.phase()==tiny2d_game::SurvivorPhase::DifficultySelection||(!limit_&&!menuInput.focused)){clock_.reset();return;}
        if(restarted){clock_.reset();offset_={};camera_.setZoom(1);}
        tiny2d_game::GameplayInput in;
        if(!limit_) {
            in.moveX=input().isKeyPressed(Key::D)-input().isKeyPressed(Key::A);
            in.moveY=input().isKeyPressed(Key::S)-input().isKeyPressed(Key::W);
        }
        const auto fb=window().framebufferSize();
        if(!limit_&&(fb.width<=0||fb.height<=0)){clock_.reset();return;}
        unsigned steps=restarted?0:limit_?1:clock_.advance(dt);
        for(unsigned i=0;i<steps;++i)session_.game().fixedUpdate(in);
        camera_.setZoom(std::clamp(camera_.zoom()*std::exp((input().isKeyPressed(Key::E)-input().isKeyPressed(Key::Q))*dt),.25f,4.f));
        offset_.x+=(input().isKeyPressed(Key::Right)-input().isKeyPressed(Key::Left))*150*dt;
        offset_.y+=(input().isKeyPressed(Key::Down)-input().isKeyPressed(Key::Up))*150*dt;
        auto p=session_.game().scene().find(session_.game().playerId())->transform.position;
        camera_.setPosition(p.x+12+offset_.x,p.y+12+offset_.y);
    }
    void render() override {
        window().clear(.025f,.04f,.06f);
        auto fb=window().framebufferSize();auto size=window().size();std::size_t draws=0,uiDraws=0;
        if(fb.width>0&&fb.height>0&&size.width>0&&size.height>0) {
            if(session_.phase()==tiny2d_game::SurvivorPhase::DifficultySelection){
                menu_.update(session_.difficulty(),size.width,size.height);
                uiCamera_.setViewportSize(size.width,size.height);uiCamera_.setPosition(size.width/2.f,size.height/2.f);
                renderer_.render(menu_.scene(),uiCamera_);return;
            }
            camera_.setViewportSize(size.width,size.height);
            renderer_.begin(camera_);renderer_.submit(grid_);renderer_.submit(session_.game().scene());renderer_.end();
            draws=renderer_.stats().drawCalls;
            hud_.update(session_.game(),size.width,size.height,session_.difficulty());
            uiCamera_.setViewportSize(size.width,size.height);uiCamera_.setPosition(size.width/2.f,size.height/2.f);
            renderer_.begin(uiCamera_);renderer_.submit(hud_.scene());renderer_.end();
            uiDraws=renderer_.stats().drawCalls;
        }
        if(session_.phase()==tiny2d_game::SurvivorPhase::DifficultySelection)return;
        ++frames_;
        double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start_).count();
        window().setTitle("Tiny2D Survivor | HP: "+std::to_string(session_.game().playerHealth())+"/"+std::to_string(session_.game().playerMaxHealth())+
            " | Kills: "+std::to_string(session_.game().killCount())+" | Enemies: "+std::to_string(session_.game().enemyCount())+
            " | Projectiles: "+std::to_string(session_.game().projectileCount())+" | Draws: "+std::to_string(draws+uiDraws)+
            " | FPS: "+std::to_string(int(frames_/std::max(.001,seconds))));
        if(limit_&&frames_>=limit_) {
            std::cout<<"Survivor frames="<<frames_<<" difficulty="<<unsigned(session_.difficulty())<<" ticks="<<session_.game().tickCount()<<" health="<<session_.game().playerHealth()
                <<" kills="<<session_.game().killCount()<<" enemies="<<session_.game().enemyCount()<<" projectiles="<<session_.game().projectileCount()
                <<" spawned="<<session_.game().spawnedCount()<<" fired="<<session_.game().firedCount()<<" hits="<<session_.game().hitCount()<<" phase="<<tiny2d_game::phaseName(session_.game().phase())<<" remaining="<<session_.game().remainingSeconds()
                <<" draws_world="<<draws<<" draws_ui="<<uiDraws<<" draws_total="<<draws+uiDraws<<'\n';
            window().requestClose();
        }
    }
    tiny2d_game::SurvivorFlowController session_;
    tiny2d_ui::DifficultyMenu menu_;
    tiny2d_ui::GameHUD hud_;
    Camera2D uiCamera_{800,600};
    Scene grid_;
    SceneRenderer renderer_;
    Camera2D camera_{800,600};
    FixedTimestep clock_;
    Vector2 offset_;
    unsigned limit_,frames_=0;
    std::chrono::steady_clock::time_point start_=std::chrono::steady_clock::now();
};
}
void runSurvivor(unsigned frames,unsigned level){Survivor app(frames,level);app.run();}
