#include "SceneRendererTestAccess.h"
#include "tiny2d/Window.h"
#include "../engine/src/Renderer/OpenGL.h"
#include <algorithm>
#include <array>

#include <chrono>
#include <iostream>
using namespace tiny2d;
using Clock=std::chrono::steady_clock;
int main(){try{
 Window w(800,600,"M5.1 preparation benchmark");glfwSwapInterval(0);Camera2D c(800,600);c.setPosition(400,300);
 std::cout<<"sprites,unique_textures,keys,preflight_median_us,total_median_us,draws,frame_owners\n";
 for(auto config:{std::array<unsigned,3>{100,1,1},{500,1,1},{1000,4,4},{1000,4,32}}){
  Scene s;SceneRenderer r;std::vector<std::shared_ptr<Texture2D>> textures;
  for(unsigned i=0;i<config[1];++i)textures.push_back(std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}}));
  for(unsigned i=0;i<config[2];++i)r.setAsset(i,textures[i%textures.size()]);
  for(unsigned i=0;i<config[0];++i){auto*o=s.find(s.create());o->sprite=SpriteData{i%config[2],{2,2}};o->transform.position={float(i%100)*8,float(i/100)*8};}
  std::vector<double> prep,total;std::size_t owners=0;
  for(unsigned i=0;i<230;++i){w.pollEvents();w.clear(0,0,0,0);auto a=Clock::now();auto count=SceneRendererTestAccess::prepare(r,s);auto b=Clock::now();r.render(s,c);auto d=Clock::now();if(i>=30){prep.push_back(std::chrono::duration<double,std::micro>(b-a).count());total.push_back(std::chrono::duration<double,std::micro>(d-b).count());}owners=count;w.swapBuffers();}
  std::sort(prep.begin(),prep.end());std::sort(total.begin(),total.end());
  std::cout<<config[0]<<','<<config[1]<<','<<config[2]<<','<<prep[prep.size()/2]<<','<<total[total.size()/2]<<','<<r.stats().drawCalls<<','<<owners<<'\n';
 }
}catch(const std::exception&e){std::cerr<<e.what();return 1;}}
