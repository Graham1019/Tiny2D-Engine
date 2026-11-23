#include "M5Test.h"
#include "Neon.h"
#include "tiny2d/RenderTarget2D.h"
#include <vector>

using namespace lab;
namespace {
Options args(std::initializer_list<const char*> list) {
    std::vector<const char*> argv{"lab"};
    argv.insert(argv.end(),list);
    return parse(int(argv.size()),argv.data());
}
Keys keys(std::initializer_list<Key> list) {
    Keys result;
    for (auto k:list) result.set(k);
    return result;
}
void input() {
    Controls c; Display d;
    auto poll=[&](Keys k,double dt=1./60){return c.poll(k,true,dt,d);};
    check(poll(keys({Pause,Bloom})).steps==0 && c.paused && !d.bloom,"pause/bloom edges");
    check(poll(keys({Pause,Bloom}),10).steps==0 && c.paused && !d.bloom,"held repeats");
    check(poll(keys({Step,Blur,Stronger})).steps==1 && d.view==View::Blur && d.strength==13,"paused step/observe");
    check(poll(keys({Step})).steps==0,"step exactly once");
    check(poll(keys({Reset})).reset && !poll(keys({Reset})).reset && c.paused,"reset edge");
    check(poll(keys({Pause}),8).steps==0 && poll({},1./60).steps==1,"no resume catch-up");
    check(poll({},1).steps==6,"bounded catch-up");
    c.poll({},false,1,d);
    auto a=poll(keys({Left,Fire,Reset,Bloom}),1);
    check(!a.steps && !a.reset && !a.motion.x && !a.motion.fire && !d.bloom,"release gate");
    poll({});
    a=poll(keys({Left,Fire,Bloom}));
    check(a.motion.x==-1 && a.motion.fire && d.bloom,"released keys rearmed");
    for(int i=0;i<60;++i) { poll({}); poll(keys({Stronger})); }
    check(d.strength==30,"bounded gain"); d.strength=-1; rejects([&]{d.validate();});
}
void simulation() {
    auto equal=[](const Neon& a,const Neon& b) {
        check(a.tick==b.tick && a.player.x==b.player.x && a.player.y==b.player.y,"pose");
        for(unsigned i=0;i<a.bolts.size();++i) {
            auto l=a.bolts[i],r=b.bolts[i];
            check(l.age==r.age && l.start.x==r.start.x && l.start.y==r.start.y &&
                l.end.x==r.end.x && l.end.y==r.end.y,"bolt state");
        }
        for(unsigned i=0;i<a.bursts.size();++i) {
            auto l=a.bursts[i],r=b.bursts[i];
            check(l.age==r.age && l.center.x==r.center.x && l.center.y==r.center.y && l.angle==r.angle,"burst rng/state");
        }
    };
    Neon a(17), b(17), other(18);
    check(a.bursts[0].angle!=other.bursts[0].angle,"seed affects presentation");
    for(int i=0;i<600;++i) a.update({1,-1,true});
    a=Neon(17); equal(a,b);
    for(int i=0;i<1800;++i) {
        Motion m{i%3-1,(i/29)%3-1,i%5!=0};
        a.update(m); b.update(m); equal(a,b);
    }
}
}
int main() { try {
    check(!args({}).frames && args({"--seed","0"}).seed==0,"defaults/zero seed");
    auto o=args({"--scene","neon","--frames","4294967295","--seed","4294967295","--bloom","off","--view","bright"});
    check(*o.frames==UINT32_MAX && o.seed==UINT32_MAX && !o.display.bloom && o.display.view==View::Bright,"CLI boundaries");
    for(auto s:{"-1","+3"," 3","1.0","1e2","2x","4294967296",""}) {
        rejects([&]{args({"--seed",s});}); rejects([&]{args({"--frames",s});});
    }
    rejects([]{args({"--frames","0"});}); rejects([]{args({"--scene","survivor"});});
    rejects([]{args({"--view","light"});}); rejects([]{args({"--bloom","true"});});
    rejects([]{args({"--frames"});}); rejects([]{args({"--size","1x0"});});
    rejects([]{tiny2d::RenderTarget2D::storageBytes(INT32_MAX,INT32_MAX);});
    input(); simulation();
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; } }
