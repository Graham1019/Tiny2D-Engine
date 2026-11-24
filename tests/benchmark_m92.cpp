// Release-only evidence fixture. The real spawn, navigation, collision and renderer run
// at a constant actor count. Survival damage, offers and terminal states do not run.
#include "NavigationPresentation.h"
#include "OpenGL.h"
#include "Pipeline.h"
#include "tiny2d_game/NavigationSession.h"
#include <fstream>
#include <iomanip>
#include <iostream>

using namespace tiny2d;
using namespace tiny2d_game;
using namespace lab;

int main(int argc,char** argv) {
    try {
        if (argc!=8) throw std::invalid_argument("width height enemies none|walk logicalWidth logicalHeight output.csv");
        MapParameters params{std::stoi(argv[1]),std::stoi(argv[2])};
        const int count=std::stoi(argv[3]);
        const std::string script=argv[4];
        if (script!="none" && script!="walk") throw std::invalid_argument("unknown movement script");
        Window window(std::stoi(argv[5]),std::stoi(argv[6]),"M9.2 navigation benchmark");
        glfwSwapInterval(0);
        const auto size=window.size(), pixels=window.framebufferSize();
        NavigationSession initial(71,params,count);
        auto player=initial.world();
        auto enemies=initial.enemies();
        auto nav=initial.navigator();
        if (int(enemies.size())!=count) throw std::runtime_error("requested load not available");
        const auto& map=player.map();
        const auto half=player.playerHalfSize();
        const auto graphUs=nav.stats().graphBuildUs, initialFieldUs=nav.stats().fieldBuildUs;
        Picture picture;
        Display display;
        display.hud=false;
        picture.worldCamera=mapCamera(map,player.playerPosition(),size);
        Pipeline pipeline;
        if (!pipeline.resize(pixels)) throw std::runtime_error("benchmark needs a drawable framebuffer");
        auto queries=openTimestampQueries(Timer::capacity);
        Timer timer(*queries);
        unsigned gpuMeasured=0;
        auto retire=[&](FrameInfo origin,QueryStatus status,const Timer::Durations&) {
            if (origin.measurement && status==QueryStatus::Complete) ++gpuMeasured;
        };
        std::ofstream out(argv[7]);
        out.exceptions(std::ios::failbit|std::ios::badbit);
        out<<std::setprecision(9)
           <<"frame,width,height,fb_width,fb_height,enemies,script,nodes,reachable,target_x,target_y,"
             "graph_us,field_us,rebuilds,reuses,visited,expanded,enemy_us,collision_us,visible_prepare_us,"
             "frame_us,visible_tiles,tile_draws,tile_bytes,submitted_enemies,enemy_draws,enemy_bytes,"
             "timestamp_bits,valid_gpu_samples\n";
        for (int frame=1;frame<=1320;++frame) {
            window.pollEvents();
            if (window.shouldClose()) throw std::runtime_error("benchmark interrupted");
            const auto start=Clock::now();
            timer.poll(retire);
            player.step(script=="walk" ? mapWalkInput(player.tick()) : MapInput{});
            const auto target=map.worldToTile(player.playerPosition());
            const auto previous=nav.stats().fieldRebuildCount;
            nav.refresh(map,player.epoch(),half,target);
            const bool rebuilt=previous!=nav.stats().fieldRebuildCount;
            double collisionUs=0;
            const auto motionStart=Clock::now();
            for (auto& enemy : enemies)
                collisionUs+=nav.advance(map,enemy,60.f/60.f);
            const double enemyUs=micros(motionStart);
            const auto prepareStart=Clock::now();
            const auto visible=prepareMapTiles(picture,map,size);
            const double prepareUs=micros(prepareStart);
            prepareMapActors(picture,map,player.playerPosition(),half,visible,false,{});
            prepareNavigationEnemies(picture,map,enemies,half,visible);
            Stats stats;
            FrameInfo info;
            info.id=frame;
            info.measurement=frame>120;
            DrawTools draw;
            if (timer.begin(info)==QueryStatus::Pending) draw.timer=&timer;
            timer.mark(0,false);
            pipeline.draw(picture,display,stats,draw);
            timer.mark(0,true);
            timer.finish(info,true);
            const double frameUs=micros(start);
            window.swapBuffers();
            if (frame==1320) { glFinish(); timer.poll(retire); }
            const auto& counters=nav.stats();
            out<<frame<<','<<map.width()<<','<<map.height()<<','<<pixels.width<<','<<pixels.height<<','
               <<count<<','<<script<<','<<map.capacity()<<','<<counters.visitedNodes<<','<<target.x<<','<<target.y<<',';
            if (frame==1) out<<graphUs;
            out<<',';
            if (rebuilt) out<<counters.fieldBuildUs;
            else if (frame==1) out<<initialFieldUs;
            out<<','<<counters.fieldRebuildCount<<','<<counters.cacheReuseCount<<','<<counters.visitedNodes<<','
               <<counters.expandedNodes<<','<<enemyUs<<','<<collisionUs<<','<<prepareUs<<','<<frameUs<<','
               <<visible.count()<<','<<stats.tileDrawCalls<<','<<stats.tileVertexBytes<<','<<stats.submittedEnemies<<','
               <<stats.enemyDrawCalls<<','<<stats.enemyVertexBytes<<','<<queries->bits<<','<<gpuMeasured<<'\n';
        }
        std::cout<<"seed=71 renderer="<<glGetString(GL_RENDERER)<<" framebuffer="<<pixels.width<<'x'<<pixels.height
                 <<" frames=1320 bits="<<queries->bits<<" completed_measurements="<<gpuMeasured<<'\n';
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
