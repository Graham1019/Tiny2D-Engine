#include "M5Test.h"
#include "Metrics.h"
#include <algorithm>
#include <filesystem>
#include <unistd.h>
#include <cmath>
#include <set>
#include <tuple>

using namespace lab;
namespace lab {
struct NeonProbe {
    static auto hidden(const Neon& n) {return std::make_tuple(n.rng_,n.nextBolt_,n.nextBurst_,n.nextRelay_);}
};
}
namespace {
void cli() {
    const char* interactive[]={"lab"};
    auto d=parse(1,interactive);
    check(d.inspector && d.timing && !d.previews,"interactive defaults");
    const char* finite[]={"lab","--frames","4"};
    d=parse(3,finite);
    check(!d.inspector && !d.timing && !d.previews,"finite defaults");
    const char* opts[]={"lab","--previews","all","--inspector","off"};
    rejects([&]{parse(5,opts);});
    for (auto name:{"--inspector","--gpu-timing","--previews","--metrics"}) {
        const char* bad[]={"lab",name,""}; rejects([&]{parse(3,bad);});
    }
}
void ownership() {
    Controls c; Display d; Keys held;
    for (auto k:{Up,Fire,Pause,Reset}) held.set(k);
    auto a=c.poll(held,true,1./60,d,true);
    check(a.steps==1 && !a.motion.fire && !a.motion.y && !a.reset && !c.paused,"capture advances clock only");
    a=c.poll(held,true,1./60,d,false);
    check(a.steps==1 && !a.motion.y && !a.motion.fire && !a.reset,"release ownership");
    c.poll({},true,0,d);
    Commands ui; ui.edges=held;
    a=c.poll(held,true,0,d,false,ui);
    check(a.reset && !c.paused && a.steps==0,"reset priority");
    c.poll({},true,0,d); ui.edges.reset(); ui.edges.set(Pause); ui.edges.set(Step);
    a=c.poll(ui.edges,true,0,d,false,ui);
    check(c.paused && !a.steps,"keyboard+button toggle once");
    c.poll({},true,0,d);
    ui.edges.reset(Pause); a=c.poll(ui.edges,true,0,d,false,ui);
    check(a.steps==1 && !c.poll(ui.edges,true,1,d).steps,"one step only");
}
void selection() {
    Neon n(71),reference(71);
    const auto before=objects(n,5);
    auto bolt=before[5].id;
    check(bolt.kind==ObjectKind::Bolt && resolve(n,5,bolt).has_value(),"active bolt");
    const auto initialBurst=before.back().id;
    for (int i=0;i<300;++i) {
        // Alternate tool visibility. Observe the actual state before each fixed update.
        if (i%2) for (auto& obj:objects(n,5)) check(bool(resolve(n,5,obj.id)),"snapshot validates");
        Motion input{i%3-1,i%5==0,true};
        n.update(input); reference.update(input);
        check(NeonProbe::hidden(n)==NeonProbe::hidden(reference) && n.tick==reference.tick &&
              n.player.x==reference.player.x && n.player.y==reference.player.y,"original simulation/RNG");
        for(unsigned j=0;j<n.bolts.size();++j) {
            auto a=n.bolts[j],b=reference.bolts[j];
            check(a.age==b.age && a.start.x==b.start.x && a.start.y==b.start.y &&
                  a.end.x==b.end.x && a.end.y==b.end.y,"bolt fields");
        }
        for(unsigned j=0;j<n.bursts.size();++j) {
            auto a=n.bursts[j],b=reference.bursts[j];
            check(a.age==b.age && a.angle==b.angle && a.center.x==b.center.x && a.center.y==b.center.y,"burst fields");
        }
    }
    check(n.bolts[0].generation>bolt.generation && !resolve(n,5,bolt) && !resolve(n,5,initialBurst),"reused slots");
    check(!resolve(n,5,{5,ObjectKind(99),0,1}) && !resolve(n,5,{5,ObjectKind::Relay,4,1}),"invalid kind/slot");
    n=Neon(71); check(!resolve(n,6,bolt),"epoch invalidation");
    Content content{13,21,{101,79}};
    tiny2d::Camera2D cam(101,79); cam.setPosition(n.player.x-2,n.player.y+3); cam.setZoom(2.5f);
    auto m=cam.viewProjectionMatrix().values;
    tiny2d::Vector2 cursor{content.x+(1+m[0]*n.player.x+m[12])*50.5f,
                          content.y+(1-m[5]*n.player.y-m[13])*39.5f};
    Presented snap{objects(n,6),cam,content,View::Scene,4,33,n.tick};
    auto pick=[&](auto c,View v,unsigned gen){return snap.pick(cursor,c,v,gen,n,6);};
    for (float density:{1.f,1.5f,2.f}) {
        // Convert native pixel positions to GLFW logical points exactly once.
        const float pixelX=(cursor.x-content.x)*density,pixelY=(cursor.y-content.y)*density;
        auto world=cam.viewportToWorld({pixelX/density,pixelY/density});
        check(std::abs(world.x-n.player.x)<.001f && std::abs(world.y-n.player.y)<.001f,"logical inverse across densities");
    }
    auto h=pick(content,View::Scene,4);
    check(h && h->kind==ObjectKind::Player,"inverse of actual projection");
    check(!pick(content,View::Final,4) && !pick(content,View::Scene,5) &&
          !pick(Content{},View::Scene,4),"view/target/size mismatch");
    cursor.x=content.x-1; check(!pick(content,View::Scene,4),"outside rect");
}
class FakeQueries : public tiny2d::TimestampQueries {
public:
    std::array<bool,8> ready{};
    std::array<unsigned,8> writes{};
    std::array<std::uint64_t,8> values{};
    unsigned reads=0;
    void stamp(unsigned i) override { ++writes.at(i); ready.at(i)=false; }
    bool available(unsigned i) override {return ready.at(i);}
    std::uint64_t result(unsigned i) override {check(ready.at(i),"read pending result"); ++reads; return values.at(i);}
};
void retirementCSV() {
    auto path=std::filesystem::temp_directory_path()/("tiny2d-m72-"+std::to_string(getpid())+".csv");
    struct Remove { std::filesystem::path path; ~Remove(){std::error_code ec;std::filesystem::remove(path,ec);} } remove{path};
    FrameInfo info; info.id=121; info.epoch=7; info.tick=81; info.timing=true; info.measurement=true;
    {
        Metrics stream(path.string());
        CpuSample cpu; cpu.info=info; cpu.status=tiny2d::QueryStatus::Pending;
        stream.record(cpu);
        GpuSample gpu{info,123,tiny2d::QueryStatus::Complete,{}};
        gpu.us[0]=2.5;
        stream.retired(gpu); // Exit before another poll or CPU frame can flush the retirement.
    }
    std::ifstream input(path);
    std::string header,cpu,gpu;
    std::getline(input,header);std::getline(input,cpu);std::getline(input,gpu);
    auto columns=[](const auto& s){return std::count(s.begin(),s.end(),',');};
    check(columns(header)==columns(cpu) && columns(cpu)==columns(gpu),"rectangular retirement CSV");
    check(cpu.find(",pending,,")!=cpu.npos && gpu.find("gpu,121,81,7,") == 0 &&
          gpu.find(",completed,123,2.5,")!=gpu.npos,"delayed CSV retains frame and empty unmeasured intervals");
}
void queries() {
    using S=tiny2d::QueryStatus;
    FakeQueries q; tiny2d::TimestampRing<FrameInfo,2,2> ring(q);
    FrameInfo f; f.epoch=1; f.id=120;
    check(ring.begin(f)==S::Off && !ring.counts.requested,"unavailable");
    q.bits=32;
    ring.begin(f); ring.mark(0,false); ring.mark(1,false); ring.mark(1,true); ring.mark(0,true); ring.finish(f,true);
    ++f.id; ++f.epoch; f.measurement=true;
    ring.begin(f); ring.mark(0,false); ring.finish(f,false);
    check(ring.counts.pending==1 && ring.counts.invalid==1,"invalid distinct from pending");
    check(ring.begin(f)==S::Skipped && q.writes[0]==1,"full ring");
    unsigned received=0;
    auto receive=[&](auto frame,S status,const auto& us){
        if (frame.epoch==1) check(status==S::Complete && !frame.measurement && *us[0]==2.5 && *us[1]==1,"delayed units/epoch");
        else check(status==S::Invalid,"partial/wrapped");
        ++received;
    };
    ring.poll(receive); check(!q.reads && !received,"asynchronous poll");
    q.ready.fill(true); q.values={100,2600,150,1150,0,0,0,0}; ring.poll(receive);
    check(received==2 && q.reads==4,"partial retires without result reads");
    ring.begin(f); ring.mark(0,false); ring.mark(0,true); ring.finish(f,true);
    q.values[0]=UINT32_MAX; q.values[1]=2; q.ready.fill(true); ring.poll(receive);
    auto c=ring.counts;
    check(c.completed==1 && c.invalid==2 && c.skipped==1 && c.pending==0 &&
          c.requested==c.completed+c.invalid+c.pending+c.skipped,"partition and wrap rejection");
}
}
int main() { try {cli(); ownership(); selection(); queries(); retirementCSV();}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;} }
