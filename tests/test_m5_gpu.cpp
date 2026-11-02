#include "M5Test.h"
#include "tiny2d/SceneRenderer.h"
#include "tiny2d/Window.h"
#include "../engine/src/Renderer/OpenGL.h"
using namespace tiny2d;
int main(){try{
 Window w(96,96,"M5 GPU");Camera2D camera(96,96);camera.setPosition(48,48);
 Scene s;SceneRenderer r;auto t=std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}});std::weak_ptr<Texture2D> weak=t;auto handle=t->nativeHandle();r.setAsset(0,t);
 auto a=s.create(),b=s.create();s.find(a)->sprite=SpriteData{0,{96,96},1,0,0,.5f};s.find(b)->sprite=SpriteData{0,{96,96},0,1,0,.5f};
 w.clear(0,0,0,0);r.begin(camera);r.submit(s);s.destroy(a);check(!weak.expired(),"shared asset");s.clear();r.removeAsset(0);t.reset();check(!weak.expired()&&glIsTexture(handle),"frame keep alive");r.end();check(weak.expired()&&!glIsTexture(handle),"flush release");check(r.stats().drawCalls==1&&r.stats().submittedSprites==2,"batch stats");
 unsigned char pixel[4];glReadBuffer(GL_BACK);glReadPixels(20,20,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);check(pixel[1]>pixel[0]&&pixel[0]>50,"transparent order");
 camera.setViewportSize(192,192);camera.setZoom(2);check(camera.viewProjectionMatrix().values[0]>0,"projection");
 auto owned=std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}});
 {struct Restore{Window&w;~Restore(){w.makeContextCurrent();}} restore{w};Window other(40,40,"M5 wrong context");rejects([&]{r.setAsset(1,owned);});rejects([&]{r.begin(camera);});}

 r.setAsset(1,owned);auto c=s.create();s.find(c)->sprite=SpriteData{1,{24,24},1,1,1,1};s.find(c)->transform.scale={2,2};
 w.clear(0,0,0,0);r.render(s,camera);auto fb=w.framebufferSize();glReadPixels(fb.width/4,fb.height*3/4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);check(pixel[0]>250,"scaled sprite projection");
 r.begin(camera);r.submit(s);
 {struct Restore{Window&w;~Restore(){w.makeContextCurrent();}} restore{w};Window other(40,40,"pending context rejection");rejects([&]{r.end();});}
 r.end();check(r.stats().drawCalls==1,"pending queue recovered");
 s.find(c)->sprite->asset=99;rejects([&]{r.render(s,camera);});s.find(c)->sprite->asset=1;r.render(s,camera);
 r.removeAsset(1);s.clear();owned.reset();check(glGetError()==GL_NO_ERROR,"GL errors");std::cout<<"Scene pixels, keep-alive, context and release passed\n";
}catch(const std::exception&e){std::cerr<<e.what();return 1;}}
