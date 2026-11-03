#include "GameSessionTestAccess.h"
#include "vfx/CombatVFX.h"
#include "ui/GameHUD.h"
#include "tiny2d/SceneRenderer.h"
#include "tiny2d/Window.h"
#include "../engine/src/Renderer/OpenGL.h"
#include <array>
using namespace tiny2d;using namespace tiny2d_game;using A=GameSessionTestAccess;
int main(){try{Window w(320,240,"M63b pixels");Camera2D cam(320,240),ui(320,240);ui.setPosition(160,120);SceneRenderer r;auto tex=std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}});auto handle=tex->nativeHandle();r.setAsset(0,tex);GameSession g(quiet());tiny2d_vfx::CombatVFX v;tiny2d_ui::GameHUD hud;
 auto pixel=[&](int x,int y){auto fb=w.framebufferSize();auto sz=w.size();std::array<unsigned char,4> p{};glReadBuffer(GL_BACK);glReadPixels(x*fb.width/sz.width,fb.height-1-y*fb.height/sz.height,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;};
 auto draw=[&]{auto size=w.size();v.present(g,size.width,size.height);w.clear(0,0,0);r.begin(cam);r.submit(g.scene());r.submit(v.scene());r.end();auto wd=r.stats();hud.update(g,size.width,size.height);r.begin(ui);r.submit(hud.scene());r.submit(v.overlay());r.end();check(wd.drawCalls==1&&wd.textureBinds==1&&r.stats().drawCalls>=1,"real pass stats");};
 auto e=A::enemy(g,{60,0});A::shot(g,{60,0});g.fixedUpdate();v.consume(g);draw();auto hit=pixel(220,120);check(hit[0]>240&&hit[1]>180,"hit yellow");A::shot(g,{60,0});g.fixedUpdate();v.consume(g);draw();check(!g.scene().find(e)&&pixel(220,120)[1]<120,"death orange ordered over hit");v.update(.2);draw();check(pixel(237,120)[0]>0&&pixel(237,120)[0]<200,"death fades");v.update(.2);draw();check(pixel(220,120)[0]==0,"death gone");
 auto shot=A::shot(g,{60,0});g.fixedUpdate();v.consume(g);draw();check(pixel(220,120)[0]>100,"trail rendered");A::erase(g,shot);draw();check(pixel(220,120)[0]>50,"trail remains after projectile deletion");v.update(.1);draw();check(pixel(220,120)[0]==0,"trail expires");
 auto contact=A::enemy(g,{0,0});g.fixedUpdate();v.consume(g);draw();check(pixel(1,100)[0]>100,"damage edge");auto hp=pixel(12,20);cam.move(3,2);cam.setZoom(2);draw();check(pixel(12,20)==hp,"HUD shake independent");
 A::erase(g,contact);
 for(int i=0;i<10001;++i){if(g.isPlaying()&&i%20==0){A::enemy(g,{60,0});A::shot(g,{60,0});A::shot(g,{60,0});}g.fixedUpdate();v.update(1./60);v.consume(g);draw();check(tex->nativeHandle()==handle&&glIsTexture(handle),"texture retained soak");}
 v.reset();draw();check(pixel(1,100)[0]==0,"reset edge");glfwSetWindowSize(glfwGetCurrentContext(),800,600);w.pollEvents();auto fb=w.framebufferSize();glViewport(0,0,fb.width,fb.height);ui.setViewportSize(800,600);ui.setPosition(400,300);draw();check(pixel(24,40)[1]>180,"resize health");
 r.begin(ui);r.submit(v.overlay());Scene invalid;invalid.find(invalid.create())->sprite=SpriteData{99,{1,1}};rejects([&]{r.submit(invalid);});r.cancel();
 {struct Restore{Window&w;~Restore(){w.makeContextCurrent();}} restore{w};Window wrong(32,32,"wrong");rejects([&]{r.render(v.scene(),cam);});}
 check(glGetError()==GL_NO_ERROR,"GL clean");std::cout<<"10001 render soak, pixels and context passed\n";
}catch(const std::exception&e){std::cerr<<e.what();return 1;}}
