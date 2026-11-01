#include "tiny2d/BatchRenderer.h"
#include "tiny2d/Window.h"
#include "../../engine/src/Renderer/OpenGL.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
using namespace tiny2d;
using Clock=std::chrono::steady_clock;
double percentile(std::vector<double> v,double q){std::sort(v.begin(),v.end());return v[static_cast<std::size_t>((v.size()-1)*q)];}
int main(int argc,char** argv){try{
    unsigned samples=120,warmup=30,repeats=5;
    std::ofstream raw("m4-frames.csv");if(!raw)throw std::runtime_error("cannot create m4-frames.csv");
    std::ofstream gpuRaw("m4-gpu-frames.csv");if(!gpuRaw)throw std::runtime_error("cannot create GPU raw data");
    gpuRaw<<"workload,sprites,renderer,repeat,frame,gpu_us\n";
    raw<<"workload,sprites,renderer,repeat,frame,cpu_us\n";
    if(argc==3&&std::string(argv[1])=="--samples")samples=std::stoul(argv[2]);else if(argc!=1)throw std::invalid_argument("usage: tiny2d_benchmark [--samples N]");
    if(samples<2||samples>10000)throw std::invalid_argument("samples 2..10000");
    Window window(800,600,"M4 benchmark");glfwSwapInterval(0);Camera2D camera(800,600);camera.setPosition(400,300);
    Texture2D texture({1,1,{255,255,255,255}});SpriteRenderer immediate;BatchRenderer batch;
    const auto framebuffer=window.framebufferSize();
    std::cerr<<"logical=800x600 framebuffer="<<framebuffer.width<<'x'<<framebuffer.height
             <<" compiler="<<__clang_version__
#ifdef NDEBUG
             <<" build=Release"
#else
             <<" build=Debug"
#endif
             <<" capacity=1000 slots="<<batch.textureSlots()<<'\n';
    const bool timer=glfwExtensionSupported("GL_ARB_timer_query");
    std::cerr<<"vendor="<<glGetString(GL_VENDOR)<<" renderer="<<glGetString(GL_RENDERER)<<" version="<<glGetString(GL_VERSION)<<" timer="<<(timer?"available":"unavailable")<<" samples="<<samples<<" warmup="<<warmup<<" repeats="<<repeats<<" vsync=0\n";
    std::cout<<"workload,sprites,renderer,repeat,cpu_p50_us,cpu_p95_us,gpu_p50_us,gpu_p95_us,gpu_samples,draw_calls,flushes,texture_binds,uploaded_bytes\n";
    for(const std::string workload:{"cpu","pixel"})for(unsigned count:{100,500,1000,5000})for(unsigned rep=0;rep<repeats;++rep)for(unsigned order=0;order<2;++order){bool useBatch=(order+rep)%2;
        std::vector<double> cpu,gpu;RenderStats stats;GLuint queries[16]{};unsigned queryFrame[16]{};bool pending[16]{};bool measured[16]{};if(timer)glGenQueries(16,queries);
        for(unsigned frame=0;frame<warmup+samples;++frame){
            window.pollEvents();window.clear(0,0,0,0);unsigned slot=frame%16;
            for(unsigned i=0;i<16&&timer;++i)if(pending[i]){GLint ready=0;glGetQueryObjectiv(queries[i],GL_QUERY_RESULT_AVAILABLE,&ready);if(ready){GLuint64 ns=0;glGetQueryObjectui64v(queries[i],GL_QUERY_RESULT,&ns);if(measured[i]){gpu.push_back(ns/1000.0);gpuRaw<<workload<<','<<count<<','<<(useBatch?"batch":"immediate")<<','<<rep<<','<<queryFrame[i]<<','<<gpu.back()<<'\n';}pending[i]=false;}}
            bool query=timer&&!pending[slot];if(query)glBeginQuery(GL_TIME_ELAPSED,queries[slot]);
            auto start=Clock::now();
            auto draw=[&](auto& r){r.beginFrame(camera);for(unsigned i=0;i<count;++i){float size=workload=="cpu"?2.f:110.f;r.draw(texture,{float(i%100)*8,float((i/100)%75)*8},{size,size},0,{1,.7f,.5f,.5f});}};
            if(useBatch){draw(batch);batch.endFrame();stats=batch.stats();}else{draw(immediate);stats=immediate.stats();}
            auto stop=Clock::now();if(query){glEndQuery(GL_TIME_ELAPSED);pending[slot]=true;measured[slot]=frame>=warmup;queryFrame[slot]=frame>=warmup?frame-warmup:0;}
            if(frame>=warmup){cpu.push_back(std::chrono::duration<double,std::micro>(stop-start).count());raw<<workload<<','<<count<<','<<(useBatch?"batch":"immediate")<<','<<rep<<','<<frame-warmup<<','<<cpu.back()<<'\n';}window.swapBuffers();
        }
        // Completion outside measured submission: drain only at trial end, never per frame.
        if(timer){glFinish();for(unsigned i=0;i<16;++i)if(pending[i]&&measured[i]){GLuint64 ns=0;glGetQueryObjectui64v(queries[i],GL_QUERY_RESULT,&ns);gpu.push_back(ns/1000.0);gpuRaw<<workload<<','<<count<<','<<(useBatch?"batch":"immediate")<<','<<rep<<','<<queryFrame[i]<<','<<gpu.back()<<'\n';}glDeleteQueries(16,queries);}
        std::cout<<workload<<','<<count<<','<<(useBatch?"batch":"immediate")<<','<<rep<<','<<percentile(cpu,.5)<<','<<percentile(cpu,.95)<<',';
        if(gpu.empty())std::cout<<"unavailable,unavailable";else std::cout<<percentile(gpu,.5)<<','<<percentile(gpu,.95);
        std::cout<<','<<gpu.size()<<','<<stats.drawCalls<<','<<stats.batchFlushes<<','<<stats.textureBinds<<','<<stats.uploadedVertexBytes<<'\n';
    }
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
