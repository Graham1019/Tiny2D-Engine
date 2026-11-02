#include "ArenaDemo.h"
#include "tiny2d/Application.h"
#include "tiny2d/SceneRenderer.h"
#include "tiny2d/FixedTimestep.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
using namespace tiny2d;
namespace {
class Arena final:public Application {
public:
    explicit Arena(unsigned limit):Application(800,600,"Tiny2D M5 Arena"),limit_(limit){
        renderer_.setAsset(0,std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}}));
        for(int i=-10;i<=10;++i){
            auto id=scene_.create();auto& o=*scene_.find(id);o.transform.position={float(i*80),-800};o.sprite=SpriteData{0,{1,1600},.15f,.2f,.25f,1,-1};
            id=scene_.create();auto& h=*scene_.find(id);h.transform.position={-800,float(i*80)};h.sprite=SpriteData{0,{1600,1},.15f,.2f,.25f,1,-1};
        }
        player_=spawn({0,0});scene_.find(player_)->sprite->g=1;
        for(unsigned i=0;i<12;++i)enemies_.push_back(spawn({float(80+ i%4*55),float(i/4*55)}));
    }
private:
    ObjectId spawn(Vector2 p){auto id=scene_.create();auto&o=*scene_.find(id);o.transform.position=p;o.sprite=SpriteData{0,{24,24},.3f,.6f,1,1,0};o.collider=Circle{{12,12},12};return id;}
    void fixed(){
        ++ticks_;
        if(ticks_%60==10&&enemies_.size()<24){enemies_.push_back(spawn({220,-100}));++created_;}
        if(ticks_%60==40&&!enemies_.empty()){auto id=enemies_.front();scene_.destroy(id);enemies_.erase(enemies_.begin());if(scene_.find(id))throw std::logic_error("stale arena ID");++deleted_;}
        auto p=scene_.find(player_)->transform.position;
        for(auto id:scene_.objects())if(auto&o=*scene_.find(id);o.sprite&&o.collider){o.sprite->r=.3f;o.sprite->g=.6f;}
        scene_.update(FixedTimestep::step,[&](Scene&s,ObjectId id,double){
            auto&o=*s.find(id);if(!o.collider)return;
            float x,y,speed;
            if(id==player_){x=input().isKeyPressed(Key::D)-input().isKeyPressed(Key::A);y=input().isKeyPressed(Key::S)-input().isKeyPressed(Key::W);speed=180;}
            else {x=p.x-o.transform.position.x;y=p.y-o.transform.position.y;speed=65;}
            float length=std::hypot(x,y);o.velocity=length>0?Vector2{x/length*speed,y/length*speed}:Vector2{};
        },[&](Scene&s,const CollisionEvent&e){++events_;if(e.phase!=CollisionPhase::Exit){s.find(e.a)->sprite->r=1;s.find(e.b)->sprite->r=1;s.find(e.a)->sprite->g=.2f;s.find(e.b)->sprite->g=.2f;}});
    }
    void update(float dt)override{
        if(input().isKeyPressed(Key::Escape))window().requestClose();
        auto fb=window().framebufferSize();if(fb.width<=0||fb.height<=0){clock_.reset();return;}
        // Finite-frame runs are deterministic simulation smoke tests.
        unsigned steps=clock_.advance(limit_?FixedTimestep::step:double(dt));for(unsigned i=0;i<steps;++i)fixed();
        camera_.setZoom(std::clamp(camera_.zoom()*std::exp((input().isKeyPressed(Key::E)-input().isKeyPressed(Key::Q))*dt),.25f,4.f));
        offset_.x+=(input().isKeyPressed(Key::Right)-input().isKeyPressed(Key::Left))*150*dt;
        offset_.y+=(input().isKeyPressed(Key::Down)-input().isKeyPressed(Key::Up))*150*dt;
        auto p=scene_.find(player_)->transform.position;camera_.setPosition(p.x+12+offset_.x,p.y+12+offset_.y);
    }
    void render()override{
        window().clear(.04f,.06f,.09f);auto fb=window().framebufferSize();auto size=window().size();std::size_t draws=0;
        if(fb.width>0&&fb.height>0&&size.width>0&&size.height>0){camera_.setViewportSize(size.width,size.height);renderer_.render(scene_,camera_);draws=renderer_.stats().drawCalls;}
        ++frames_;double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start_).count();
        window().setTitle("Tiny2D M5 | FPS: "+std::to_string(int(frames_/std::max(seconds,.001)))+" | Draws: "+std::to_string(draws)+" | Objects: "+std::to_string(scene_.count())+" | Contacts: "+std::to_string(scene_.contacts()));
        if(limit_&&frames_>=limit_){if(limit_>=120&&(!created_||!deleted_||!events_||!draws||ticks_!=frames_))throw std::runtime_error("Arena smoke did not exercise lifecycle/collision/rendering");std::cout<<"Arena frames="<<frames_<<" ticks="<<ticks_<<" draws="<<draws<<" objects="<<scene_.count()<<" contacts="<<scene_.contacts()<<" events="<<events_<<" created="<<created_<<" deleted="<<deleted_<<'\n';window().requestClose();}
    }
    Scene scene_;
    SceneRenderer renderer_;
    Camera2D camera_{800,600};
    FixedTimestep clock_;
    ObjectId player_;std::vector<ObjectId> enemies_;Vector2 offset_;
    unsigned limit_,frames_=0,ticks_=0,created_=0,deleted_=0,events_=0;
    std::chrono::steady_clock::time_point start_=std::chrono::steady_clock::now();
};
}
void runArena(unsigned frames){Arena arena(frames);arena.run();}
