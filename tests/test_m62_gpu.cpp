#include "GameSessionTestAccess.h"
#include "tiny2d_game/SessionController.h"
#include "ui/GameHUD.h"
#include "tiny2d/SceneRenderer.h"
#include "tiny2d/Window.h"
#include "../engine/src/Renderer/OpenGL.h"
#include <array>
using namespace tiny2d;
using namespace tiny2d_game;
int main(){try{
    Window w(320,240,"M62 pixels");Camera2D world(320,240),ui(320,240);ui.setPosition(160,120);
    SceneRenderer renderer;auto tex=std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}});auto handle=tex->nativeHandle();renderer.setAsset(0,tex);
    tiny2d_ui::GameHUD hud;SessionController session(quiet(),{1});
    auto pixel=[&](int x,int y){auto fb=w.framebufferSize();std::array<unsigned char,4> p{};glReadBuffer(GL_BACK);glReadPixels(x*fb.width/320,fb.height-1-y*fb.height/240,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;};
    auto draw=[&]{w.clear(0,0,0);renderer.render(session.game().scene(),world);auto wd=renderer.stats().drawCalls;hud.update(session.game(),320,240);renderer.render(hud.scene(),ui);check(wd==1&&renderer.stats().drawCalls==(hud.scene().count()+999)/1000,"actual two pass draws");};
    draw();check(pixel(12,20)[1]>180&&pixel(74,20)[1]>180,"full health bar");check(pixel(10,6)[0]>240&&pixel(11,6)[0]<40,"H glyph bitmap");auto stable=pixel(12,20);
    world.move(500,100);world.setZoom(3);draw();check(pixel(12,20)==stable,"UI independent");
    session.game().fixedUpdate();draw();check(pixel(119,80)[0]>200,"victory title");
    check(session.restartKey(true),"restart");draw();check(pixel(119,80)[0]==0,"overlay cleared");
    auto c=quiet();c.playerMaxHealth=1;GameSession death(c);GameSessionTestAccess::enemy(death,{0,0});death.fixedUpdate();hud.update(death,320,240);w.clear(0,0,0);renderer.render(hud.scene(),ui);check(pixel(12,20)[1]<80,"empty health bar");check(pixel(109,80)[0]>200,"game over glyph");
    for(int i=0;i<150;++i){session.game().fixedUpdate();session.restartKey(false);session.restartKey(true);draw();check(tex->nativeHandle()==handle&&glIsTexture(handle),"GPU resource reused");}
    // Resize logical projection while preserving the framebuffer's own pixel size.
    glfwSetWindowSize(glfwGetCurrentContext(),800,600);w.pollEvents();
    auto size=w.size();auto fb=w.framebufferSize();glViewport(0,0,fb.width,fb.height);
    ui.setViewportSize(size.width,size.height);ui.setPosition(size.width/2.f,size.height/2.f);
    hud.update(session.game(),size.width,size.height);w.clear(0,0,0);renderer.render(hud.scene(),ui);
    std::array<unsigned char,4> resized{};
    glReadPixels(24*fb.width/size.width,fb.height-1-40*fb.height/size.height,1,1,GL_RGBA,GL_UNSIGNED_BYTE,resized.data());
    check(resized[1]>180,"resized logical health bar");
    renderer.begin(ui);renderer.submit(hud.scene());Scene invalid;auto& bad=*invalid.find(invalid.create());bad.sprite=SpriteData{99,{10,10}};
    rejects([&]{renderer.submit(invalid);});renderer.cancel();
    renderer.render(hud.scene(),ui);check(glGetError()==GL_NO_ERROR,"GL clean");std::cout<<"M62 GPU pixels passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
