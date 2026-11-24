#include "GameSessionTestAccess.h"
#include "EnhancedSurvivor.h"
#include "OpenGL.h"
#include "SurvivorTrace.h"
#include "tiny2d_game/SessionController.h"
#include "ui/DifficultyMenu.h"

#include <array>

using namespace tiny2d;
using namespace tiny2d_game;
using namespace tiny2d_ui;
namespace {
using Pixels=std::vector<std::uint8_t>;
Pixels pixels(Size size) {
    Pixels result(std::size_t(size.width)*size.height*4);
    glReadBuffer(GL_BACK);
    glReadPixels(0,0,size.width,size.height,GL_RGBA,GL_UNSIGNED_BYTE,result.data());
    detail::checkGL("read Survivor pixels");
    return result;
}
std::array<std::uint8_t,3> sample(const Pixels& p,Size fb,Size logical,int x,int y) {
    const auto offset=4*((fb.height-1-y*fb.height/logical.height)*fb.width+x*fb.width/logical.width);
    return {p[offset],p[offset+1],p[offset+2]};
}
void run() {
    Window w(320,240,"M11.1 Survivor HDR");
    SceneRenderer r;
    r.setAsset(0,std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}}));
    Scene grid;
    Camera2D world(320,240),ui(320,240);
    world.setPosition(0,0);
    EnhancedSurvivor enhanced(true);
    auto config=quiet();
    config.playerMaxHealth=2;
    SessionController flow(config,{8});
    auto& game=flow.game();
    tiny2d_vfx::CombatVFX vfx;
    GameHUD hud;
    const auto enemy=GameSessionTestAccess::enemy(game,{40,30});
    GameSessionTestAccess::shot(game,{40,30});
    GameSessionTestAccess::shot(game,{40,30});
    GameSessionTestAccess::enemy(game,{0,0});
    game.fixedUpdate();
    check(!game.scene().find(enemy)&&game.playerHealth()==1,"real kill and damage");
    vfx.consume(game);
    DifficultyMenu menu;
    Size logical=w.size(), fb=w.framebufferSize();
    enhanced.resize(fb);
    menu.update(DifficultyLevel::Normal,logical.width,logical.height);
    ui.setPosition(logical.width/2.f,logical.height/2.f);
    w.clear(0,0,0);
    r.render(menu.scene(),ui);
    const auto classicMenu=pixels(fb);
    enhanced.menu(r,menu.scene(),ui);
    const auto enhancedMenu=pixels(fb);
    for (int y=24;y<190;y+=3) for (int x=32;x<284;x+=7)
        check(sample(classicMenu,fb,logical,x,y)==sample(enhancedMenu,fb,logical,x,y),"opaque menu pixels changed");

    for (int width:{320,384}) {
        glfwSetWindowSize(w.nativeHandle(),width,width*3/4);
        w.pollEvents();
        logical=w.size();fb=w.framebufferSize();
        enhanced.resize(fb);
        const auto state=survivorTraceRow(game,0);
        prepareSurvivor(game,vfx,hud,world,ui,logical.width,logical.height,DifficultyLevel::Normal);
        w.clear(.025f,.04f,.06f);
        survivorWorld(r,world,grid,game,vfx);
        survivorOverlay(r,ui,hud,vfx);
        const auto classic=pixels(fb);
        glEnable(GL_SCISSOR_TEST);
        glViewport(3,5,17,19);
        auto draw=[&] { return enhanced.draw(r,grid,game,vfx,hud,world,ui); };
        auto stats=draw();
        GLint vp[4];glGetIntegerv(GL_VIEWPORT,vp);
        check(glIsEnabled(GL_SCISSOR_TEST)&&vp[0]==3&&vp[3]==19,"frame reset caller's GL state");
        check(!r.passActive()&&enhanced.stats().complete&&stats.world.drawCalls>0&&stats.ui.drawCalls>0,
              "production world and overlay complete");
        const auto bloom=pixels(fb);
        enhanced.bloomKey(true,true); enhanced.bloomKey(true,true);
        check(!enhanced.bloom(),"single edge disables bloom");
        enhanced.bloomKey(true,false); enhanced.bloomKey(true,true);
        check(!enhanced.bloom(),"focus release gate");
        draw();
        const auto off=pixels(fb);
        check(!(enhanced.stats().mask&(1<<lab::Extract))&&enhanced.stats().fullscreen==1,"bloom off skips extraction/blur");
        check(bloom!=off&&classic!=off,"HDR extraction and mapping change real pixels");
        const int x=logical.width*15/320,y=logical.height*21/240;
        auto hp=sample(bloom,fb,logical,x,y);
        check(hp==sample(off,fb,logical,x,y)&&hp==sample(classic,fb,logical,x,y)&&hp[1]>hp[0],
              "opaque health bar after tone mapping retains classic RGB at both scales");
        enhanced.bloomKey(false,true);enhanced.bloomKey(true,true);enhanced.bloomKey(false,true);
        draw();
        check(bloom==pixels(fb)&&enhanced.stats().fullscreen==4,"re-enable restores same image with four fullscreen draws");
        check(enhanced.stats().passDraws[lab::Overlay]==stats.ui.drawCalls,"post-output pass counts HUD and damage edge");
        auto& bad=*grid.find(grid.create());
        bad.sprite=SpriteData{9};
        rejects(draw);
        check(!enhanced.stats().complete&&!r.passActive(),"failed world cancels pass and records incomplete frame");
        grid.clear();
        draw();
        check(bloom==pixels(fb),"retry after failing layer has identical pixels");
        check(survivorTraceRow(game,0)==state,"render changed game or combat events");
        glDisable(GL_SCISSOR_TEST);glViewport(0,0,fb.width,fb.height);
    }
    check(!enhanced.resize({0,0}),"suspended zero framebuffer");
    rejects([&]{enhanced.draw(r,grid,game,vfx,hud,world,ui);});
    enhanced.resize(fb);
    { Window foreign(32,32,"foreign Survivor context");rejects([&]{enhanced.resize(fb);}); }
    w.makeContextCurrent();
    while(game.isPlaying()) game.fixedUpdate();
    check(game.phase()==GamePhase::Victory,"terminal victory");
    prepareSurvivor(game,vfx,hud,world,ui,logical.width,logical.height,DifficultyLevel::Normal);
    enhanced.draw(r,grid,game,vfx,hud,world,ui);
    check(flow.restartKey(true)&&flow.game().tickCount()==0,"restart through production controller");
    vfx.consume(flow.game());
    prepareSurvivor(flow.game(),vfx,hud,world,ui,logical.width,logical.height,DifficultyLevel::Normal);
    enhanced.draw(r,grid,flow.game(),vfx,hud,world,ui);
    check(enhanced.stats().complete&&vfx.active()==0,"restart clears VFX and draws fresh live game");
    std::cout<<"verified exact HUD/menu, HDR/bloom on/off, GL scope, resize "<<logical.width<<" to "
             <<fb.width<<" framebuffer pixels, terminal and restart\n";
}
}
int main() { try { run(); } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; } }
