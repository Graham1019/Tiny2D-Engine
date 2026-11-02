#include "M5Test.h"
#include "SceneRendererTestAccess.h"
#include "tiny2d/Window.h"
#include "../engine/src/Renderer/OpenGL.h"
#include <limits>
#include <array>
using namespace tiny2d;
namespace {
auto state(){
 std::vector<GLint> result;
 for(auto name:{GL_CURRENT_PROGRAM,GL_VERTEX_ARRAY_BINDING,GL_ARRAY_BUFFER_BINDING,GL_ACTIVE_TEXTURE,GL_BLEND_SRC_RGB,GL_BLEND_DST_RGB,GL_BLEND_SRC_ALPHA,GL_BLEND_DST_ALPHA,GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA}){GLint v;glGetIntegerv(name,&v);result.push_back(v);}
 result.push_back(glIsEnabled(GL_BLEND));GLint unit;glGetIntegerv(GL_ACTIVE_TEXTURE,&unit);
 for(unsigned i=0;i<8;++i){glActiveTexture(GL_TEXTURE0+i);GLint v;glGetIntegerv(GL_TEXTURE_BINDING_2D,&v);result.push_back(v);}glActiveTexture(unit);return result;
}
auto pixels(Window& w){auto f=w.framebufferSize();std::vector<unsigned char> p(f.width*f.height*4);glReadBuffer(GL_BACK);glReadPixels(0,0,f.width,f.height,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;}
ObjectId sprite(Scene&s,uint32_t asset=0){auto id=s.create();s.find(id)->sprite=SpriteData{asset,{64,64},1,.2f,.3f,.3f};return id;}
void inputFailures(Window&w,Camera2D&camera){
 Scene s;SceneRenderer r;auto t=std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}});r.setAsset(0,t);
 sprite(s);sprite(s);auto last=sprite(s);auto*o=s.find(last);
 auto rejectUnchanged=[&]{auto before=pixels(w);auto bindings=state();auto count=t.use_count();auto stats=r.stats();rejects([&]{r.render(s,camera);});check(pixels(w)==before&&state()==bindings,"preflight changed pixels/state");check(t.use_count()==count&&SceneRendererTestAccess::owners(r)==0,"failed preflight retained owners");check(r.stats().submittedSprites==stats.submittedSprites,"preflight began pass");};
 w.clear(.07f,.1f,.2f,.4f);o->sprite->asset=99;rejectUnchanged();o->sprite->asset=0;
 r.render(s,camera);auto retried=pixels(w);w.clear(.07f,.1f,.2f,.4f);r.render(s,camera);check(retried==pixels(w),"failed render caused duplicate alpha");
 const auto good=*o->sprite;const auto transform=o->transform;
 for(int mode=0;mode<6;++mode){
  *o->sprite=good;o->transform=transform;
  if(mode==0)o->sprite->size.x=0;
  if(mode==1)o->transform.scale.x=std::numeric_limits<float>::infinity();
  if(mode==2){o->sprite->size.x=std::numeric_limits<float>::max();o->transform.scale.x=2;}
  if(mode==3)o->sprite->a=std::numeric_limits<float>::quiet_NaN();
  if(mode==4){o->transform.position.x=std::numeric_limits<float>::max();o->sprite->size.x=std::numeric_limits<float>::max();}
  if(mode==5)o->transform.rotation=std::numeric_limits<float>::quiet_NaN();
  rejectUnchanged();
 }
 *o->sprite=good;o->transform=transform;
 // More than capacity AND slots: trailing invalid data must prevent every draw.
 for(unsigned i=1;i<10;++i)r.setAsset(i,std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}}));
 for(unsigned i=0;i<1002;++i)sprite(s,i%10);auto bad=sprite(s,99);rejectUnchanged();s.destroy(bad);
 r.render(s,camera);check(r.stats().drawCalls>1,"boundary scene did not auto flush");
}
void explicitPass(Window&w,Camera2D&camera){
 Scene good,bad;SceneRenderer r;auto t=std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}});r.setAsset(0,t);sprite(good);sprite(bad);sprite(bad,99);
 w.clear(0,0,0,0);auto before=pixels(w);auto bindings=state();r.begin(camera);r.submit(good);auto active=state();auto stats=r.stats();rejects([&]{r.submit(bad);});
 check(state()==active&&pixels(w)==before&&r.stats().submittedSprites==stats.submittedSprites,"explicit validation changed queue/state");
 r.end();auto expected=pixels(w);w.clear(0,0,0,0);r.render(good,camera);check(expected==pixels(w),"invalid submit appended sprites");
 w.clear(0,0,0,0);r.begin(camera);r.submit(good);rejects([&]{r.submit(bad);});r.cancel();check(pixels(w)==before&&state()==bindings&&r.stats().drawCalls==0,"cancel flushed or changed state");rejects([&]{r.cancel();});
}
void lifetime(Window&w,Camera2D&camera){
 Scene s;SceneRenderer r;auto old=std::make_shared<Texture2D>(ImageData{1,1,{255,0,0,255}});auto replacement=std::make_shared<Texture2D>(ImageData{1,1,{0,255,0,255}});
 std::weak_ptr<Texture2D> a=old,b=replacement;auto ha=old->nativeHandle(),hb=replacement->nativeHandle();r.setAsset(0,old);r.setAsset(1,old);
 for(int i=0;i<100;++i)sprite(s,i%2);r.begin(camera);r.submit(s);check(SceneRendererTestAccess::owners(r)==1&&old.use_count()==4,"owners not deduplicated across keys");
 r.setAsset(0,replacement);r.removeAsset(1);for(auto id:s.objects())s.find(id)->sprite->asset=0;r.submit(s);check(SceneRendererTestAccess::owners(r)==2,"replacement lost old texture");
 r.removeAsset(0);old.reset();replacement.reset();s.clear();check(!a.expired()&&!b.expired(),"pending owners released");
 {struct Restore{Window&w;~Restore(){w.makeContextCurrent();}} restore{w};Window other(32,32,"wrong cancel context");auto bindings=state();rejects([&]{r.cancel();});rejects([&]{r.end();});check(state()==bindings&&!a.expired()&&!b.expired(),"wrong context changed state/ownership");}
 r.cancel();check(a.expired()&&b.expired()&&!glIsTexture(ha)&&!glIsTexture(hb),"cancel leaked textures");
 // Actual batch cancellation also preserves already-flushed work.
 Texture2D t({1,1,{255,255,255,255}});BatchRenderer batch(1,1);w.clear(0,0,0,0);auto bindings=state();batch.beginFrame(camera);batch.draw(t,{0,0},{64,64},0,{1,0,0,1});batch.draw(t,{0,0},{64,64},0,{0,1,0,1});auto flushed=pixels(w);check(batch.stats().drawCalls==1,"expected auto flush");
 {struct Restore{Window&w;~Restore(){w.makeContextCurrent();}} restore{w};Window other(32,32,"wrong batch cancel context");rejects([&]{batch.cancelFrame();});}
 batch.cancelFrame();check(pixels(w)==flushed&&state()==bindings&&batch.stats().drawCalls==1,"cancel rolled back or flushed");rejects([&]{batch.cancelFrame();});batch.beginFrame(camera);batch.endFrame();
}
}
int main(){try{Window w(64,64,"M5.1 regression");Camera2D camera(64,64);camera.setPosition(32,32);inputFailures(w,camera);explicitPass(w,camera);lifetime(w,camera);check(glGetError()==GL_NO_ERROR,"GL error");std::cout<<"M5.1 preflight RGBA/state, explicit failure, boundaries, owners and cancel passed\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
