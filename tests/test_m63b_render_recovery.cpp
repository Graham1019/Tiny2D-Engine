#include "GameSessionTestAccess.h"
#include "SurvivorPresentation.h"
#include "tiny2d/Window.h"
#include "../engine/src/Renderer/OpenGL.h"
#include <array>
#include <cstdlib>
#include <new>
static bool failAllocation=false;
void* operator new(std::size_t n){if(failAllocation)throw std::bad_alloc();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
static auto glState(){std::vector<GLint> s;for(auto name:{GL_CURRENT_PROGRAM,GL_VERTEX_ARRAY_BINDING,GL_ARRAY_BUFFER_BINDING,GL_ACTIVE_TEXTURE,GL_BLEND_SRC_RGB,GL_BLEND_DST_RGB,GL_BLEND_SRC_ALPHA,GL_BLEND_DST_ALPHA,GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA}){GLint x;glGetIntegerv(name,&x);s.push_back(x);}s.push_back(glIsEnabled(GL_BLEND));GLint unit;glGetIntegerv(GL_ACTIVE_TEXTURE,&unit);for(unsigned i=0;i<8;++i){glActiveTexture(GL_TEXTURE0+i);GLint x;glGetIntegerv(GL_TEXTURE_BINDING_2D,&x);s.push_back(x);}glActiveTexture(unit);return s;}
using namespace tiny2d;using namespace tiny2d_ui;using namespace tiny2d_game;
int main(){try{
 Window w(160,160,"Pass recovery");Camera2D c(160,160);c.setPosition(80,80);SceneRenderer r;auto tex=std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}});r.setAsset(0,tex);
 Scene red,green,bad;auto make=[](Scene& s,float x,float rr,float gg,unsigned asset){auto& o=*s.find(s.create());o.transform.position={x,60};o.sprite=SpriteData{asset,{20,20},rr,gg,0,1,0};};make(red,20,1,0,0);make(green,100,0,1,0);make(bad,20,1,0,99);
 auto pixel=[&](int x){auto fb=w.framebufferSize();std::array<unsigned char,4> p{};glReadBuffer(GL_BACK);glReadPixels(x*fb.width/160,fb.height-1-70*fb.height/160,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;};
 auto recover=[&]{check(!r.passActive(),"inactive after exception");w.clear(0,0,0);{RenderPass pass(r,c);r.submit(green);pass.end();}check(r.stats().drawCalls==1&&pixel(30)[0]==0&&pixel(110)[1]>240,"no stale commands");};
 auto initialState=glState();
 for(int passIndex=0;passIndex<2;++passIndex){w.clear(0,0,0);if(passIndex)r.render(green,c);bool caught=false;try{RenderPass pass(r,c);r.submit(red);r.submit(bad);pass.end();}catch(const std::invalid_argument& e){caught=std::string(e.what())=="missing scene texture asset";}check(caught,"original preflight error");check(glState()==initialState,"guard restores GL bindings/blend");check(pixel(30)[0]==0,"cancel unflushed world/ui");if(passIndex)check(pixel(110)[1]>240,"completed world remains");recover();}
 r.begin(c);rejects([&]{RenderPass nested(r,c);});check(r.passActive(),"failed guard construction does not cancel caller pass");r.cancel();recover();
 // Failure after a capacity flush cannot undo completed draws, only pending commands.
 Scene many;for(int i=0;i<1001;++i)make(many,20,1,0,0);w.clear(0,0,0);try{RenderPass pass(r,c);r.submit(many);r.submit(bad);}catch(const std::invalid_argument&){}check(pixel(30)[0]>240&&r.stats().drawCalls==1,"already flushed draw preserved");recover();
 // A real GL error consumed by end's upload check exercises its runtime cleanup.
 bool endFailed=false;try{RenderPass pass(r,c);r.submit(red);glEnable(0xffffffffu);pass.end();}catch(const std::runtime_error& e){
#ifdef NDEBUG
 endFailed=std::string(e.what()).find("batch upload: OpenGL error(s) 1280")!=std::string::npos;
#else
 endFailed=std::string(e.what()).find("Texture2D::requireCurrentContext: OpenGL error(s) 1280")!=std::string::npos;
#endif
}check(endFailed,"end fault preserved its actual validation diagnostic");recover();
 // Exact production preparation path rejects an invalid viewport before begin.
 GameSession game(quiet());tiny2d_vfx::CombatVFX vfx;GameHUD hud;Camera2D ui(160,160);Scene grid;
 rejects([&]{renderSurvivor(r,grid,game,vfx,hud,c,ui,0,160,DifficultyLevel::Casual);});check(!r.passActive(),"prepare failure before pass");renderSurvivor(r,grid,game,vfx,hud,c,ui,160,160,DifficultyLevel::Casual);recover();
 // Real CPU allocation failure after VFX preparation, before either pass.
 bool allocationFailed=false;failAllocation=true;
 try{renderSurvivor(r,grid,game,vfx,hud,c,ui,161,160,DifficultyLevel::Casual);}catch(const std::bad_alloc&){allocationFailed=true;}
 failAllocation=false;check(allocationFailed&&!r.passActive(),"CPU allocation failure leaves no pass");
 renderSurvivor(r,grid,game,vfx,hud,c,ui,160,160,DifficultyLevel::Casual);recover();
 // Wrong-context cancellation intentionally fails. Restore owner and explicitly cancel.
 Window other(32,32,"other");w.makeContextCurrent();bool contextFailed=false;
 try{RenderPass pass(r,c);r.submit(red);other.makeContextCurrent();pass.end();}catch(const std::logic_error& e){contextFailed=std::string(e.what()).find("wrong SceneRenderer context")!=std::string::npos;}
 check(contextFailed&&r.passActive(),"no false wrong-context recovery");w.makeContextCurrent();r.cancel();recover();
 // Pending ownership is released by the guard, not leaked into the next frame.
 std::weak_ptr<Texture2D> weak=tex;try{RenderPass pass(r,c);r.submit(red);r.removeAsset(0);tex.reset();r.submit(bad);}catch(const std::invalid_argument&){}check(weak.expired(),"cancel releases pending owners");r.setAsset(0,std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}}));recover();
 check(glGetError()==GL_NO_ERROR,"clean GL");
}catch(const std::exception&e){failAllocation=false;std::cerr<<e.what();return 1;}}
