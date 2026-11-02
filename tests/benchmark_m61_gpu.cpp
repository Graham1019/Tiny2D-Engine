#include "GameSessionTestAccess.h"
#include "tiny2d/SceneRenderer.h"
#include "tiny2d/Window.h"
#include <chrono>
using namespace tiny2d;
using namespace tiny2d_game;
using A=GameSessionTestAccess;
int main() {
    Window window(800,600,"M6.1 submission benchmark");Camera2D camera(800,600);
    SceneRenderer renderer;renderer.setAsset(0,std::make_shared<Texture2D>(ImageData{1,1,{255,255,255,255}}));
    std::cout<<"enemies,projectiles,sample,submission_us,draws\n";
    for(auto workload:{std::pair<int,int>{8,8},{24,32},{48,64}}) {
        GameSession g(quiet());
        for(int i=0;i<workload.first;++i)A::enemy(g,{float(i%12*32),float(i/12*32)});
        for(int i=0;i<workload.second;++i)A::shot(g,{float(i%12*32),float(-40-i/12*12)});
        for(int i=-30;i<300;++i) {
            window.clear(0,0,0);auto start=std::chrono::steady_clock::now();
            renderer.render(g.scene(),camera);auto end=std::chrono::steady_clock::now();
            if(i>=0)std::cout<<workload.first<<','<<workload.second<<','<<i<<','
                <<std::chrono::duration<double,std::micro>(end-start).count()<<','<<renderer.stats().drawCalls<<'\n';
        }
    }
}
