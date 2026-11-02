#include "SurvivorDemo.h"
#include "tiny2d/Application.h"
#include "tiny2d/SceneRenderer.h"
#include "tiny2d/FixedTimestep.h"
#include "tiny2d_game/GameSession.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
using namespace tiny2d;
namespace {
class Survivor final:public Application {
public:
    explicit Survivor(unsigned limit):Application(800,600,"Tiny2D Survivor"),limit_(limit) {
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
        tiny2d_game::GameplayInput in;
        if(!limit_) {
            in.moveX=input().isKeyPressed(Key::D)-input().isKeyPressed(Key::A);
            in.moveY=input().isKeyPressed(Key::S)-input().isKeyPressed(Key::W);
        }
        const auto fb=window().framebufferSize();
        if(!limit_&&(fb.width<=0||fb.height<=0)){clock_.reset();return;}
        unsigned steps=limit_?1:clock_.advance(dt);
        for(unsigned i=0;i<steps;++i)game_.fixedUpdate(in);
        camera_.setZoom(std::clamp(camera_.zoom()*std::exp((input().isKeyPressed(Key::E)-input().isKeyPressed(Key::Q))*dt),.25f,4.f));
        offset_.x+=(input().isKeyPressed(Key::Right)-input().isKeyPressed(Key::Left))*150*dt;
        offset_.y+=(input().isKeyPressed(Key::Down)-input().isKeyPressed(Key::Up))*150*dt;
        auto p=game_.scene().find(game_.playerId())->transform.position;
        camera_.setPosition(p.x+12+offset_.x,p.y+12+offset_.y);
    }
    void render() override {
        window().clear(.025f,.04f,.06f);
        auto fb=window().framebufferSize();auto size=window().size();std::size_t draws=0;
        if(fb.width>0&&fb.height>0&&size.width>0&&size.height>0) {
            camera_.setViewportSize(size.width,size.height);
            renderer_.begin(camera_);renderer_.submit(grid_);renderer_.submit(game_.scene());renderer_.end();
            draws=renderer_.stats().drawCalls;
        }
        ++frames_;
        double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start_).count();
        window().setTitle("Tiny2D Survivor | HP: "+std::to_string(game_.playerHealth())+"/"+std::to_string(game_.playerMaxHealth())+
            " | Kills: "+std::to_string(game_.killCount())+" | Enemies: "+std::to_string(game_.enemyCount())+
            " | Projectiles: "+std::to_string(game_.projectileCount())+" | Draws: "+std::to_string(draws)+
            " | FPS: "+std::to_string(int(frames_/std::max(.001,seconds))));
        if(limit_&&frames_>=limit_) {
            std::cout<<"Survivor frames="<<frames_<<" ticks="<<game_.tickCount()<<" health="<<game_.playerHealth()
                <<" kills="<<game_.killCount()<<" enemies="<<game_.enemyCount()<<" projectiles="<<game_.projectileCount()
                <<" spawned="<<game_.spawnedCount()<<" fired="<<game_.firedCount()<<" hits="<<game_.hitCount()<<" draws="<<draws<<'\n';
            window().requestClose();
        }
    }
    tiny2d_game::GameSession game_;
    Scene grid_;
    SceneRenderer renderer_;
    Camera2D camera_{800,600};
    FixedTimestep clock_;
    Vector2 offset_;
    unsigned limit_,frames_=0;
    std::chrono::steady_clock::time_point start_=std::chrono::steady_clock::now();
};
}
void runSurvivor(unsigned frames){Survivor app(frames);app.run();}
