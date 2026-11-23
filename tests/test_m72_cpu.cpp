#include "M5Test.h"
#include "Metrics.h"
#include <algorithm>
#include <filesystem>
#include <unistd.h>
#include <cmath>
#include <set>
#include <sstream>
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
    for (auto effect:galleryEffects) {
        const char* args[]={"lab","--effect",galleryName(effect),"--scene","shaders"};
        const auto gallery=parse(5,args);
        check(gallery.scene==LabScene::Shaders && gallery.effect==effect &&
              !gallery.display.lighting,"gallery CLI effect and lighting");
    }
    const char* badEffect[]={"lab","--scene","shaders","--effect","missing"};
    const char* wrongScene[]={"lab","--scene","particles","--effect","aurora"};
    const char* wrongView[]={"lab","--scene","shaders","--view","shadow"};
    for (const auto* args:{badEffect,wrongScene,wrongView}) rejects([&]{parse(5,args);});
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
    std::array<bool,Timer::capacity> ready{};
    std::array<unsigned,Timer::capacity> writes{};
    std::array<std::uint64_t,Timer::capacity> values{};
    unsigned reads=0;
    void stamp(unsigned i) override { ++writes.at(i); ready.at(i)=false; }
    bool available(unsigned i) override {return ready.at(i);}
    std::uint64_t result(unsigned i) override {check(ready.at(i),"read pending result"); ++reads; return values.at(i);}
};
struct TempCSV {
    std::filesystem::path path=std::filesystem::temp_directory_path()/("tiny2d-m72-"+std::to_string(getpid())+".csv");
    ~TempCSV() { std::error_code ec; std::filesystem::remove(path,ec); }
};
void retirementCSV() {
    TempCSV file;
    FrameInfo info; info.id=121; info.epoch=7; info.tick=81; info.timing=true; info.measurement=true;
    {
        Metrics stream(file.path.string());
        CpuSample cpu; cpu.info=info; cpu.status=tiny2d::QueryStatus::Pending;
        stream.record(cpu);
        GpuSample gpu{info,123,tiny2d::QueryStatus::Complete,{}};
        gpu.us[0]=2.5;
        stream.retired(gpu); // Exit before another poll or CPU frame can flush the retirement.
    }
    std::ifstream input(file.path);
    std::string header,cpu,gpu;
    std::getline(input,header);std::getline(input,cpu);std::getline(input,gpu);
    auto columns=[](const auto& s){return std::count(s.begin(),s.end(),',');};
    check(columns(header)==columns(cpu) && columns(cpu)==columns(gpu),"rectangular retirement CSV");
    check(cpu.find(",pending,,")!=cpu.npos && gpu.find("gpu,121,81,7,") == 0 &&
          gpu.find(",completed,123,2.5,")!=gpu.npos,"delayed CSV retains frame and empty unmeasured intervals");
}
void appendedTimestampCSV(bool gallery) {
    static_assert(ShadowInspect==15 && ParticleEmission==16 && ShaderGalleryPass==17);
    const auto pass=gallery ? ShaderGalleryPass : ParticleEmission;
    FakeQueries queries;
    queries.bits=32;
    Timer ring(queries);
    CpuSample cpu;
    auto& info=cpu.info;
    info.timing=true;
    info.particles=info.demoScript=!gallery;
    if (gallery) {
        info.galleryEffect=GalleryEffect::Contours;
        info.shaderVersion=12;
    }
    info.seed=83;
    info.particleSteady=0;
    info.particleCapacity=8192;
    info.particleRate=720;
    cpu.status=ring.begin(info);
    ring.mark(0,false);
    ring.mark(pass+1,false);
    ring.mark(pass+1,true);
    ring.mark(0,true);
    ring.finish(info,true);
    queries.ready.fill(true);
    queries.values[1]=5000;
    queries.values[(pass+1)*2]=100;
    queries.values[(pass+1)*2+1]=2100;
    TempCSV file;
    Metrics csv(file.path.string());
    cpu.stats.mask=1u<<pass;
    cpu.stats.cpuUs[pass]=3.5;
    csv.record(cpu);
    ring.poll([&](auto frame,auto status,const auto& durations) {
        check(status==tiny2d::QueryStatus::Complete && durations[pass+1]==2.,
              "appended pass interval retired in microseconds");
        csv.retired({frame,2,status,durations});
    });
    csv.close();
    std::ifstream stream(file.path);
    auto row=[&] {
        std::string line;
        check(bool(std::getline(stream,line)),"missing CSV record");
        std::vector<std::string> fields;
        std::istringstream in(line+",end"); // Preserve trailing empty fields.
        for (std::string field;std::getline(in,field,',');) fields.push_back(field);
        fields.pop_back();
        return fields;
    };
    auto header=row(), submitted=row(), retired=row();
    check(header.size()==submitted.size() && header.size()==retired.size(),"CPU/GPU CSV widths");
    auto col=[&](const char* name) {
        auto it=std::find(header.begin(),header.end(),name);
        check(it!=header.end(),"missing CSV column");
        return std::size_t(it-header.begin());
    };
    check(col("update_us")==42 && col("world_draws")==58 && col("fullscreen")==121 &&
          col("particle_emission_us")==122,"M7 schema indices retained");
    if (gallery) {
        check(col("shader_gallery_us")==col("valid_gpu_samples")+1 &&
              submitted[col("shader_gallery_us")]=="3.5" && retired[col("shader_gallery_us")]=="2" &&
              retired[col("scene")]=="shaders" && retired[col("gallery_effect")]=="contours" &&
              retired[col("shader_version")]=="12" && retired[col("particle_emission_us")].empty() &&
              retired[col("particle_steady")].empty(),"gallery issuing effect, version and pass on retirement");
    } else check(submitted[col("particle_emission_us")]=="3.5" && retired[col("particle_emission_us")]=="2" &&
          retired[col("scene")]=="particles" && retired[col("seed")]=="83" &&
          retired[col("particle_steady")]=="0" && retired[col("particle_script")]=="demo" &&
          retired[col("instance_upload_us")].empty() && retired[col("shadow_inspect_us")].empty(),
          "particle stats, config and unmeasured columns aligned");
}
void orderedGpuHistory() {
    using S=tiny2d::QueryStatus;
    using Ring=tiny2d::TimestampRing<FrameInfo,PassCount+1,2>;
    FakeQueries q;
    q.bits=32;
    Ring ring(q);
    TempCSV file;
    Metrics csv(file.path.string());
    History history;
    std::uint64_t epoch=1;
    std::vector<std::uint64_t> retired;
    auto issue=[&](unsigned id,bool complete=true) {
        FrameInfo f;
        f.id=id; f.epoch=epoch; f.tick=epoch==1 ? 100+id : id-7;
        auto status=ring.begin(f);
        if (status==S::Pending) {
            ring.mark(0,false);
            if (complete) ring.mark(0,true);
            ring.finish(f,complete);
        }
        return status;
    };
    auto poll=[&](unsigned received,std::initializer_list<unsigned> readySlots) {
        for (auto slot:readySlots) {
            unsigned start=slot*(Ring::capacity/2);
            q.ready[start]=q.ready[start+1]=true;
        }
        ring.poll([&](FrameInfo f,S status,const Timer::Durations& us) {
            GpuSample sample{f,received,status,us};
            retired.push_back(f.id);
            csv.retired(sample);
            history.recordGpu(epoch,sample);
        });
        csv.flushRetired();
        const auto& c=ring.counts;
        check(c.requested==c.completed+c.pending+c.skipped+c.invalid,"history poll counts partition");
    };

    issue(1); issue(2); poll(3,{0});
    issue(3); poll(4,{0,1}); // Reused slot 0 retires ahead of older slot 1.
    check(retired==std::vector<std::uint64_t>{1,3,2},"real ring retirement order");
    check(history.gpu.size()==3,"all three completed frames enter history");
    std::cout<<"retired=1,3,2 latest="<<history.gpu.back().info.id<<'\n';
    check(history.gpu.back().info.id==3 && history.gpu[1].info.id==2 &&
          history.gpu.back().received==4,"latest must be frame 3 after retiring 1,3,2");

    issue(4); issue(5); poll(6,{1});
    check(history.gpu.size()==4 && history.gpu.back().info.id==5,"newest frame completes first");
    poll(7,{0});
    check(history.gpu.size()==5 && history.gpu.back().info.id==5 && history.gpu[3].info.id==4,
          "cross-poll late frame");

    issue(6); // Old epoch stays pending across Reset.
    ++epoch; history={};
    issue(7); check(issue(8)==S::Skipped,"full ring skips frame 8");
    poll(9,{1});
    issue(9,false);
    check(ring.counts.pending==1 && ring.counts.invalid==1,"partial counts before retirement");
    poll(10,{0,1});
    check(history.gpu.size()==1 && history.gpu.back().info.id==7 && history.gpu.back().info.epoch==epoch,
          "late old epoch and invalid record excluded from current history");

    issue(10); // Keep the oldest slot pending beyond the entire 120-frame window.
    for (unsigned id=11;id<=133;++id) { issue(id); poll(id+1,{1}); }
    poll(134,{0});
    poll(135,{}); // Retired slots must not emit twice.
    check(history.gpu.size()==120,"bounded GPU history");
    for (unsigned i=0;i<120;++i) check(history.gpu[i].info.id==14+i,"retains newest 120 originating frames");
    const auto& c=ring.counts;
    check(c.requested==133 && c.completed==131 && c.pending==0 && c.skipped==1 && c.invalid==1,
          "history leaves lifetime query counts intact");

    csv.close();
    std::ifstream input(file.path);
    std::string line;
    std::getline(input,line); // Header.
    std::set<unsigned> csvIds;
    while (std::getline(input,line)) {
        std::istringstream fields(line);
        std::string kind,idText,tickText,epochText;
        std::getline(fields,kind,','); std::getline(fields,idText,',');
        std::getline(fields,tickText,','); std::getline(fields,epochText,',');
        const auto id=std::stoul(idText);
        check(kind=="gpu" && id>=1 && id<=133 && id!=8 && csvIds.insert(id).second,"one row per retirement");
        check(std::stoul(epochText)==(id<=6?1:2) && std::stoul(tickText)==(id<=6?100+id:id-7),
              "CSV preserves issuing epoch and tick");
        check(line.find(id==9?",invalid,":",completed,")!=line.npos,"CSV retirement status");
    }
    check(csvIds.size()==c.completed+c.invalid,"CSV includes every retired frame, even after Reset or eviction");
    std::cout<<"cross-poll latest=5, reset latest=7, window=14..133, requested=133 completed=131 "
                "pending=0 skipped=1 invalid=1, unique GPU CSV rows=132\n";
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
int main() { try {cli(); ownership(); selection(); queries(); retirementCSV(); appendedTimestampCSV(false); appendedTimestampCSV(true); orderedGpuHistory();}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;} }
