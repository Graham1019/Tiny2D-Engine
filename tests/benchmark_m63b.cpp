#include "GameSessionTestAccess.h"
#include "tiny2d_game/Difficulty.h"
#include "tiny2d_game/SessionController.h"
#include "ui/GameHUD.h"
#include "ui/DifficultyMenu.h"
#include "tiny2d/SceneRenderer.h"
#include "tiny2d/Window.h"
#include "../engine/src/Renderer/OpenGL.h"
#ifdef M63B
#include "vfx/CombatVFX.h"
#endif
#include <chrono>
using namespace tiny2d;using namespace tiny2d_game;using A=GameSessionTestAccess;
using Clock=std::chrono::steady_clock;
static double us(Clock::time_point a,Clock::time_point b){return std::chrono::duration<double,std::micro>(b-a).count();}
int main(){Window w(800,600,"M63b profile");Camera2D world(800,600),ui(800,600);ui.setPosition(400,300);SceneRenderer r;r.setAsset(0,std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}}));
 std::cout<<"workload,sample,fixed_us,scene_proxy_us,vfx_update_us,vfx_present_us,hud_us,world_us,ui_us,frame_cpu_us,finish_wait_us,frame_completed_us,world_draws,ui_draws,sprites,flushes,binds,bytes,enemies,projectiles,vfx,objects\n";
 for(int load=0;load<9;++load){auto cfg=load<3?makeDifficultyConfig(difficultyLevel(load*2+1)):quiet();SessionController session(cfg,{load==5?1ull:5400ull});auto& g=session.game();
 if(load==3){for(int i=0;i<48;++i)A::enemy(g,{float(i*32+100),100});for(int i=0;i<64;++i)A::shot(g,{float(i*16+100),-100},10000);}
 if(load==5)g.fixedUpdate();if(load==6){A::enemy(g,{0,0});step(g,145);}if(load==7){step(g,5400);session.restartKey(true);}
 Scene proxy;for(auto id:session.game().scene().objects()){auto n=proxy.create();proxy.find(n)->transform=session.game().scene().find(id)->transform;proxy.find(n)->collider=session.game().scene().find(id)->collider;}
 tiny2d_ui::GameHUD hud;tiny2d_ui::DifficultyMenu menu;
#ifdef M63B
 tiny2d_vfx::CombatVFX vfx;
#endif
 for(int i=-30;i<100;++i){auto& game=session.game();if(load==4){for(int j=0;j<8;++j){A::enemy(game,{float(j*30),80});A::shot(game,{float(j*30),80});A::shot(game,{float(j*30),80});}}
 auto p0=Clock::now();proxy.update(1./60);auto p1=Clock::now();auto frameBegin=Clock::now();w.clear(0,0,0);auto a=Clock::now();if(load!=8)game.fixedUpdate({1,0});auto b=Clock::now();
#ifdef M63B
 if(load!=8){vfx.update(1./60);vfx.consume(game);}
#endif
 auto c=Clock::now();
#ifdef M63B
 if(load!=8)vfx.present(game,800,600);
#endif
 auto d=Clock::now();if(load==8)menu.update(DifficultyLevel::Normal,800,600);else hud.update(game,800,600,load<3?difficultyLevel(load*2+1):DifficultyLevel::Casual);auto e=Clock::now();RenderStats wd;
 if(load!=8){r.begin(world);r.submit(game.scene());
#ifdef M63B
 r.submit(vfx.scene());
#endif
 r.end();wd=r.stats();}auto f=Clock::now();r.begin(ui);r.submit(load==8?menu.scene():hud.scene());
#ifdef M63B
 if(load!=8)r.submit(vfx.overlay());
#endif
 r.end();auto ud=r.stats();auto h=Clock::now();glFinish();auto j=Clock::now();unsigned active=0;
#ifdef M63B
 active=vfx.active();
#endif
 if(i>=0)std::cout<<load<<','<<i<<','<<us(a,b)<<','<<us(p0,p1)<<','<<us(b,c)<<','<<us(c,d)<<','<<us(d,e)<<','<<us(e,f)<<','<<us(f,h)<<','<<us(frameBegin,h)<<','<<us(h,j)<<','<<us(frameBegin,j)<<','<<wd.drawCalls<<','<<ud.drawCalls<<','<<wd.submittedSprites+ud.submittedSprites<<','<<wd.batchFlushes+ud.batchFlushes<<','<<wd.textureBinds+ud.textureBinds<<','<<wd.uploadedVertexBytes+ud.uploadedVertexBytes<<','<<game.enemyCount()<<','<<game.projectileCount()<<','<<active<<','<<game.scene().count()<<'\n';
 }
 }}
