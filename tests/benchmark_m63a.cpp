#include "GameSessionTestAccess.h"
#include "tiny2d_game/Difficulty.h"
#include "ui/GameHUD.h"
#include "ui/DifficultyMenu.h"
#include "tiny2d/SceneRenderer.h"
#include "tiny2d/Window.h"
#include "../engine/src/Renderer/OpenGL.h"
#include <chrono>
using namespace tiny2d;using namespace tiny2d_game;
using Clock=std::chrono::steady_clock;
static double us(Clock::time_point a,Clock::time_point b){return std::chrono::duration<double,std::micro>(b-a).count();}
int main(int argc,char** argv){bool stress=argc==2&&std::string(argv[1])=="--stress";Window w(800,600,"M63a measurements");Camera2D world(800,600),ui(800,600);ui.setPosition(400,300);SceneRenderer r;r.setAsset(0,std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}}));
 std::cout<<"level,opportunity,tick,spawned,kills,enemies,projectiles,fixed_us,hud_us,world_submit_us,ui_submit_us,gpu_drain_us,world_draws,ui_draws,menu_us\n";
 for(unsigned d=1;d<=5;++d){auto config=makeDifficultyConfig(difficultyLevel(d));
 // Stress-only health budget keeps all five schedules active for a full 90 seconds.
 if(stress)config.playerMaxHealth=100000;
 GameSession g(config);tiny2d_ui::GameHUD hud;tiny2d_ui::DifficultyMenu menu;
 for(int t=1;t<=5400;++t){auto a=Clock::now();g.fixedUpdate({1,0});auto b=Clock::now();hud.update(g,800,600,difficultyLevel(d));auto c=Clock::now();r.render(g.scene(),world);auto e=Clock::now();auto wd=r.stats().drawCalls;r.render(hud.scene(),ui);auto f=Clock::now();auto ud=r.stats().drawCalls;glFinish();auto h=Clock::now();menu.update(difficultyLevel(d),800,600);auto j=Clock::now();
 std::cout<<d<<','<<t<<','<<g.tickCount()<<','<<g.spawnedCount()<<','<<g.killCount()<<','<<g.enemyCount()<<','<<g.projectileCount()<<','<<us(a,b)<<','<<us(b,c)<<','<<us(c,e)<<','<<us(e,f)<<','<<us(f,h)<<','<<wd<<','<<ud<<','<<us(h,j)<<'\n';}
 }}
