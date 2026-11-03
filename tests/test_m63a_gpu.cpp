#include "GameSessionTestAccess.h"
#include "tiny2d_game/SurvivorFlowController.h"
#include "ui/DifficultyMenu.h"
#include "ui/GameHUD.h"
#include "tiny2d/SceneRenderer.h"
#include "tiny2d/Window.h"
#include "../engine/src/Renderer/OpenGL.h"
#include <array>
using namespace tiny2d;using namespace tiny2d_game;
int main(){try{
 Window w(320,240,"M63a pixels");Camera2D world(320,240),ui(320,240);ui.setPosition(160,120);
 SceneRenderer renderer;auto tex=std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}});auto handle=tex->nativeHandle();renderer.setAsset(0,tex);
 SurvivorFlowController flow;tiny2d_ui::DifficultyMenu menu;tiny2d_ui::GameHUD hud;
 auto pixel=[&](int x,int y){auto fb=w.framebufferSize();auto size=w.size();std::array<unsigned char,4> p{};glReadBuffer(GL_BACK);glReadPixels(x*fb.width/size.width,fb.height-1-y*fb.height/size.height,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;};
 auto draw=[&]{auto s=w.size();ui.setViewportSize(s.width,s.height);ui.setPosition(s.width/2.f,s.height/2.f);w.clear(0,0,0);
 if(flow.phase()==SurvivorPhase::DifficultySelection){menu.update(flow.difficulty(),s.width,s.height);renderer.render(menu.scene(),ui);}
 else{renderer.render(flow.game().scene(),world);auto wd=renderer.stats().drawCalls;hud.update(flow.game(),s.width,s.height,flow.difficulty());renderer.render(hud.scene(),ui);check(wd==1&&renderer.stats().drawCalls==(hud.scene().count()+999)/1000,"world/ui accounting");}};
 draw();check(pixel(42,113)[2]>100&&pixel(42,69)[2]<40,"default level3 highlight");check(pixel(40,22)[0]>200,"font pixels");
 MenuInput input;input.levels[4]=true;flow.input(input);draw();check(pixel(42,157)[2]>100&&pixel(42,113)[2]<40,"selection pixels change");
 input.enter=true;flow.input(input);draw();check(pixel(160,120)[2]>200,"player world appears");check(pixel(10,228)[0]>200,"LV glyph appears");auto label=pixel(10,228);world.move(500,100);world.setZoom(3);draw();check(pixel(10,228)==label,"HUD camera independence");
 auto labelPixels=[&]{std::vector<std::array<unsigned char,4>> result;for(int y=228;y<235;++y)for(int x=10;x<100;++x)result.push_back(pixel(x,y));return result;};
 auto actualLabel=labelPixels();
 auto revision=hud.revision();hud.update(flow.game(),320,240,DifficultyLevel::Casual);check(hud.revision()>revision,"difficulty cache invalidation");w.clear(0,0,0);renderer.render(hud.scene(),ui);check(labelPixels()!=actualLabel,"level text changes pixels");draw();check(labelPixels()==actualLabel,"actual level restored");
 for(int i=0;i<10;++i){step(flow.game(),5400);draw();input.restart=false;flow.input(input);input.restart=true;check(flow.input(input),"restart");draw();check(flow.difficulty()==DifficultyLevel::Extreme&&pixel(10,228)==label&&tex->nativeHandle()==handle&&glIsTexture(handle),"same difficulty GPU reused");}
 // Keep the actual level config; remove enemies in the fixture to reach Victory safely.
 for(int t=0;t<5400;++t){std::vector<ObjectId> enemies;for(auto [id,e]:flow.game().entities())if(e.kind==EntityKind::Enemy)enemies.push_back(id);for(auto id:enemies)GameSessionTestAccess::erase(flow.game(),id);flow.game().fixedUpdate();}
 check(flow.game().phase()==GamePhase::Victory,"level5 victory");draw();input.restart=false;flow.input(input);input.restart=true;check(flow.input(input),"victory restart");draw();check(labelPixels()==actualLabel,"victory retains label");
 glfwSetWindowSize(glfwGetCurrentContext(),800,600);w.pollEvents();auto fb=w.framebufferSize();glViewport(0,0,fb.width,fb.height);draw();check(pixel(20,576)[0]>200,"resize HUD");menu.update(DifficultyLevel::Normal,800,600);renderer.render(menu.scene(),ui);check(pixel(164,286)[2]>100,"resize menu highlight");
 renderer.begin(ui);renderer.submit(menu.scene());{struct Restore{Window&w;~Restore(){w.makeContextCurrent();}} restore{w};Window other(32,32,"wrong context");rejects([&]{renderer.end();});rejects([&]{renderer.cancel();});}renderer.end();
 check(glGetError()==GL_NO_ERROR,"no GL error");
}catch(const std::exception&e){std::cerr<<e.what();return 1;}}
