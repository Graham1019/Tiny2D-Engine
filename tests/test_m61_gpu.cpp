#include <array>

#include "GameSessionTestAccess.h"
#include "tiny2d/SceneRenderer.h"
#include "tiny2d/Window.h"
#include "../engine/src/Renderer/OpenGL.h"
using namespace tiny2d;
using namespace tiny2d_game;
using A=GameSessionTestAccess;
int main(){try {
    Window w(160,160,"M6.1 GPU");Camera2D camera(160,160);camera.setPosition(0,0);
    auto c=quiet();GameSession game(c);auto enemy=A::enemy(game,{40,0});auto shot=A::shot(game,{-40,0},2);
    SceneRenderer renderer;auto texture=std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}});
    auto handle=texture->nativeHandle();std::weak_ptr<Texture2D> weak=texture;renderer.setAsset(0,texture);
    auto pixel=[&](int x) {
        auto fb=w.framebufferSize();std::array<unsigned char,4> p{};
        glReadBuffer(GL_BACK);glReadPixels((x+80)*fb.width/160,fb.height/2,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;
    };
    w.clear(0,0,0);renderer.render(game.scene(),camera);
    auto blue=pixel(0),red=pixel(40),yellow=pixel(-40);
    check(blue[2]>200&&blue[0]<60&&red[0]>200&&red[1]<80&&yellow[0]>200&&yellow[1]>200,"entity tint pixels");
    check(renderer.stats().submittedSprites==3&&renderer.stats().drawCalls==1,"shared texture batch");
    A::shot(game,{40,0});A::shot(game,{40,0});game.fixedUpdate();game.fixedUpdate();
    check(!game.scene().find(enemy)&&!game.scene().find(shot)&&game.killCount()==1,"combat cleanup");
    w.clear(0,0,0);renderer.render(game.scene(),camera);
    check(pixel(40)[0]==0&&pixel(-40)[0]==0&&renderer.stats().submittedSprites==1,"deleted sprites disappear");
    A::enemy(game,{0,0});game.fixedUpdate();w.clear(0,0,0);renderer.render(game.scene(),camera);
    check(game.playerHealth()==4&&game.scene().find(game.playerId())->sprite->r==1,"damage sprite sync");
    renderer.begin(camera);renderer.submit(game.scene());renderer.removeAsset(0);texture.reset();
    check(!weak.expired()&&glIsTexture(handle),"pending keep alive");
    {struct Restore{Window& w;~Restore(){w.makeContextCurrent();}} restore{w};Window other(32,32,"wrong");
        rejects([&]{renderer.end();});rejects([&]{renderer.cancel();});}
    renderer.end();check(weak.expired()&&!glIsTexture(handle),"texture release");
    rejects([&]{renderer.render(game.scene(),camera);});
    renderer.setAsset(0,std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}}));
    renderer.begin(camera);renderer.submit(game.scene());renderer.cancel();
    check(renderer.stats().drawCalls==0,"cancel does not flush");
    renderer.render(game.scene(),camera);check(glGetError()==GL_NO_ERROR,"GL errors");
    std::cout<<"survivor GPU pixels, cleanup, keep-alive and contexts passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
