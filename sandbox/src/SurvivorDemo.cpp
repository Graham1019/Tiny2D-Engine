#include "SurvivorDemo.h"
#include "EnhancedSurvivor.h"
#include "SurvivorTrace.h"
#include "Capture.h"
#include "tiny2d/Application.h"
#include "tiny2d/FixedTimestep.h"
#include "tiny2d_game/SurvivorFlowController.h"
#include "ui/DifficultyMenu.h"
#include "FrameRate.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <GLFW/glfw3.h>

using namespace tiny2d;
namespace {
using Clock=std::chrono::steady_clock;
double since(Clock::time_point start) {
    return std::chrono::duration<double,std::micro>(Clock::now()-start).count();
}

class Survivor final:public Application {
public:
    explicit Survivor(const SurvivorOptions& options)
        :Application(800,600,"Tiny2D Survivor"),session_(tiny2d_game::difficultyLevel(options.difficulty)),
         options_(options) {
        if (options_.frames) session_.start();
        if (options_.enhanced) enhanced_=std::make_unique<tiny2d_ui::EnhancedSurvivor>(options_.bloom);
        if (!options_.trace.empty()) {
            trace_=std::make_unique<SurvivorCsv>(options_.trace);
            trace_->line(survivorTraceHeader());
        }
        if (!options_.metrics.empty()) {
            metrics_=std::make_unique<SurvivorCsv>(options_.metrics);
            metrics_->line("frame,tick,mode,bloom,fixed_us,prepare_us,world_us,ui_us,draw_us,extract_us,"
                           "horizontal_us,vertical_us,output_us,world_draws,ui_draws,fullscreen_draws,"
                           "pass_mask,hdr_bytes,framebuffer_width,framebuffer_height,gpu_us");
        }
        if (options_.frames && (options_.enhanced || trace_ || metrics_ || !options_.capture.empty())) glfwSwapInterval(0);
        renderer_.setAsset(0,std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}}));
        for(int i=-20;i<=20;++i) {
            auto id=grid_.create();auto& o=*grid_.find(id);
            o.transform.position={float(i*80),-1600};o.sprite=SpriteData{0,{1,3200},.075f,.10f,.14f,1,-1};
            id=grid_.create();auto& h=*grid_.find(id);
            h.transform.position={-1600,float(i*80)};h.sprite=SpriteData{0,{3200,1},.075f,.10f,.14f,1,-1};
        }
    }
private:
    void update(float dt) override {
        if(input().isKeyPressed(Key::Escape))window().requestClose();
        tiny2d_game::MenuInput menuInput;
        for(unsigned i=0;i<5;++i)menuInput.levels[i]=input().isKeyPressed('1'+i);
        menuInput.enter=input().isKeyPressed(257);menuInput.restart=input().isKeyPressed('R');menuInput.focused=input().focused();
        const bool finite=options_.frames!=0;
        if (enhanced_ && !finite) enhanced_->bloomKey(input().isKeyPressed('B'),menuInput.focused,window().takeFocusLoss());
        bool restarted=finite?false:session_.input(menuInput);
        if(session_.phase()==tiny2d_game::SurvivorPhase::DifficultySelection||(!finite&&!menuInput.focused)){clock_.reset();return;}
        if(restarted){vfx_.reset();clock_.reset();offset_={};camera_.setZoom(1);}
        tiny2d_game::GameplayInput in;
        if(!finite) {
            in.moveX=input().isKeyPressed(Key::D)-input().isKeyPressed(Key::A);
            in.moveY=input().isKeyPressed(Key::S)-input().isKeyPressed(Key::W);
        }
        const auto fb=window().framebufferSize();
        if(!finite&&(fb.width<=0||fb.height<=0)){clock_.reset();return;}
        unsigned steps=restarted?0:finite?1:clock_.advance(dt);
        vfx_.update(finite?FixedTimestep::step:dt);
        auto& game=session_.game();
        fixedUs_=0;
        for(unsigned i=0;i<steps;++i){
            const auto previousTick=game.tickCount(),previousEvent=game.eventSequence();
            const auto start=metrics_?Clock::now():Clock::time_point{};
            game.fixedUpdate(in);
            if (metrics_) fixedUs_+=since(start);
            vfx_.consume(game);
            if (trace_ && previousTick!=game.tickCount()) trace_->line(survivorTraceRow(game,previousEvent));
        }
        vfx_.consume(game);
        // The legacy interactive controls are still applied by the same controller.
        // Finite presentation evidence uses the fixed default camera.
        if (!finite || (!trace_ && !metrics_ && options_.capture.empty() && !enhanced_)) {
            camera_.setZoom(std::clamp(camera_.zoom()*std::exp((input().isKeyPressed(Key::E)-input().isKeyPressed(Key::Q))*dt),.25f,4.f));
            offset_.x+=(input().isKeyPressed(Key::Right)-input().isKeyPressed(Key::Left))*150*dt;
            offset_.y+=(input().isKeyPressed(Key::Down)-input().isKeyPressed(Key::Up))*150*dt;
        }
        auto p=game.scene().find(game.playerId())->transform.position;
        auto shake=vfx_.shake();
        camera_.setPosition(p.x+12+offset_.x+shake.x/camera_.zoom(),p.y+12+offset_.y+shake.y/camera_.zoom());
    }

    void render() override {
        window().clear(.025f,.04f,.06f);
        const auto fb=window().framebufferSize(),size=window().size();
        const bool drawable=fb.width>0&&fb.height>0&&size.width>0&&size.height>0;
        fps_.sample(std::chrono::duration<double>(Clock::now().time_since_epoch()).count(),drawable&&input().focused());
        window().setTitle("Tiny2D Survivor | FPS: "+std::to_string(int(fps_.value())));
        tiny2d_ui::SurvivorRenderStats stats;
        if (enhanced_) enhanced_->resize(drawable?fb:Size{});
        if (drawable) {
            if(session_.phase()==tiny2d_game::SurvivorPhase::DifficultySelection){
                menu_.update(session_.difficulty(),size.width,size.height);
                uiCamera_.setViewportSize(size.width,size.height);uiCamera_.setPosition(size.width/2.f,size.height/2.f);
                if (enhanced_) enhanced_->menu(renderer_,menu_.scene(),uiCamera_);
                else renderer_.render(menu_.scene(),uiCamera_);
                return;
            }
            const auto prepared=metrics_?Clock::now():Clock::time_point{};
            tiny2d_ui::prepareSurvivor(session_.game(),vfx_,hud_,camera_,uiCamera_,size.width,size.height,session_.difficulty());
            const auto drawStart=metrics_?Clock::now():Clock::time_point{};
            double worldUs=0,uiUs=0;
            if (enhanced_) {
                stats=enhanced_->draw(renderer_,grid_,session_.game(),vfx_,hud_,camera_,uiCamera_);
                worldUs=enhanced_->stats().cpuUs[lab::World];
                uiUs=enhanced_->stats().cpuUs[lab::Overlay];
            } else {
                stats.world=tiny2d_ui::survivorWorld(renderer_,camera_,grid_,session_.game(),vfx_);
                const auto middle=metrics_?Clock::now():Clock::time_point{};
                stats.ui=tiny2d_ui::survivorOverlay(renderer_,uiCamera_,hud_,vfx_);
                if (metrics_) {
                    worldUs=std::chrono::duration<double,std::micro>(middle-drawStart).count();
                    uiUs=since(middle);
                }
            }
            if (metrics_) recordMetrics(fb,stats,std::chrono::duration<double,std::micro>(drawStart-prepared).count(),
                                        worldUs,uiUs,since(drawStart));
        }
        if(session_.phase()==tiny2d_game::SurvivorPhase::DifficultySelection)return;
        const auto draws=stats.world.drawCalls,uiDraws=stats.ui.drawCalls;
        const auto total=enhanced_ && drawable?enhanced_->stats().draws:draws+uiDraws;
        window().setTitle("Tiny2D Survivor | FPS: "+std::to_string(int(fps_.value()))+
            " | HP: "+std::to_string(session_.game().playerHealth())+
            " | Kills: "+std::to_string(session_.game().killCount())+
            " | VFX: "+std::to_string(vfx_.active())+" | Draws: "+std::to_string(total)+
            (enhanced_?(enhanced_->bloom()?" | B: Bloom on":" | B: Bloom off"):""));
        ++frames_;
        if(options_.frames&&frames_>=options_.frames) {
            if (!options_.capture.empty()) {
                if (!drawable) throw std::runtime_error("Survivor capture requires a drawable framebuffer");
                lab::captureNew(options_.capture,fb);
            }
            if (trace_) trace_->finish();
            if (metrics_) metrics_->finish();
            std::cout<<"Survivor frames="<<frames_<<" difficulty="<<unsigned(session_.difficulty())<<" ticks="<<session_.game().tickCount()<<" health="<<session_.game().playerHealth()
                <<" kills="<<session_.game().killCount()<<" enemies="<<session_.game().enemyCount()<<" projectiles="<<session_.game().projectileCount()
                <<" spawned="<<session_.game().spawnedCount()<<" fired="<<session_.game().firedCount()<<" hits="<<session_.game().hitCount()<<" phase="<<tiny2d_game::phaseName(session_.game().phase())<<" remaining="<<session_.game().remainingSeconds()
                <<" draws_world="<<draws<<" draws_ui="<<uiDraws<<" draws_total="<<total;
            if (enhanced_) std::cout<<" presentation=enhanced bloom="<<enhanced_->bloom()<<" hdr_bytes="<<enhanced_->storage();
            std::cout<<'\n';
            window().requestClose();
        }
    }

    void recordMetrics(Size fb,const tiny2d_ui::SurvivorRenderStats& scene,double prep,double world,double ui,double draw) {
        lab::Stats classic;
        const auto& s=enhanced_?enhanced_->stats():classic;
        std::ostringstream row;
        row<<frames_+1<<','<<session_.game().tickCount()<<','<<(enhanced_?"enhanced":"classic")<<','
           <<(enhanced_&&enhanced_->bloom())<<','<<fixedUs_<<','<<prep<<','<<world<<','<<ui<<','<<draw<<','
           <<s.cpuUs[lab::Extract]<<','<<s.cpuUs[lab::Horizontal]<<','<<s.cpuUs[lab::Vertical]<<','
           <<s.cpuUs[lab::Output]<<','<<scene.world.drawCalls<<','<<scene.ui.drawCalls<<','<<s.fullscreen<<','
           <<s.mask<<','<<(enhanced_?enhanced_->storage():0)<<','<<fb.width<<','<<fb.height<<",N/A";
        metrics_->line(row.str());
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
    SurvivorOptions options_;
    unsigned frames_=0;
    double fixedUs_=0;
    tiny2d_vfx::CombatVFX vfx_;
    tiny2d_ui::FrameRate fps_;
    std::unique_ptr<tiny2d_ui::EnhancedSurvivor> enhanced_;
    std::unique_ptr<SurvivorCsv> trace_,metrics_;
};
}

void runSurvivor(const SurvivorOptions& options){Survivor app(options);app.run();}
void runSurvivor(unsigned frames,unsigned level) {
    SurvivorOptions options;
    options.frames=frames;
    options.difficulty=level;
    runSurvivor(options);
}
