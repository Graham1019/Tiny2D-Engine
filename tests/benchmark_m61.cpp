#include "GameSessionTestAccess.h"
#include <chrono>
#include <algorithm>
#include <iomanip>
using namespace tiny2d_game;
using A=GameSessionTestAccess;
int main() {
    using Clock=std::chrono::steady_clock;
    std::cout<<"enemies,projectiles,pairs,sample,fixed_us,scene_collision_us\n";
    for(auto workload:{std::pair<int,int>{8,8},{24,32},{48,64}}) {
        auto c=quiet();c.projectileLifetime=86400;GameSession g(c);
        for(int i=0;i<workload.first;++i)A::enemy(g,{float(100+i*32),100});
        for(int i=0;i<workload.second;++i)A::shot(g,{float(100+i*32),-100},100000);
        tiny2d::Scene collision;
        for(auto id:g.scene().objects()) {
            auto n=collision.create();collision.find(n)->transform=g.scene().find(id)->transform;
            collision.find(n)->collider=g.scene().find(id)->collider;
        }
        auto count=g.scene().count();
        for(int i=-30;i<300;++i) {
            auto start=Clock::now();g.fixedUpdate();auto mid=Clock::now();collision.update(1.0/60);auto end=Clock::now();
            if(i>=0)std::cout<<workload.first<<','<<workload.second<<','<<count*(count-1)/2<<','<<i<<','
                <<std::chrono::duration<double,std::micro>(mid-start).count()<<','
                <<std::chrono::duration<double,std::micro>(end-mid).count()<<'\n';
        }
    }
}
