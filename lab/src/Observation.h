#pragma once
#include "GalleryEffect.h"
#include "Presentation.h"
#include "tiny2d/TimestampQueries.h"
#include <algorithm>
#include <deque>

namespace lab {
struct FrameInfo {
    std::uint64_t id=0,tick=0,epoch=0,generation=0;
    tiny2d::Size pixels{},logical{};
    Display display;
    bool inspector=false,timing=false,measurement=false;
    unsigned previews=0,mask=0;
    int timestampBits=-1;
    bool particles=false, demoScript=false;
    std::uint32_t seed=71, particleCapacity=0, particleRate=0;
    std::optional<std::uint32_t> particleSteady;
    std::optional<GalleryEffect> galleryEffect;
    std::uint64_t shaderVersion=0;
    int mapWidth=0,mapHeight=0;
    bool mapWalk=false, navigation=false, replay=false;
    std::uint64_t revision=0, eventSequence=0, recordedEvents=0, verifiedEvents=0, simulationHash=0;
    const char* replayMode="";
};
using Timer=tiny2d::TimestampRing<FrameInfo,PassCount+1>;
struct MapFrameSample {
    double generationUs=0,validationUs=0,collisionUs=0,visiblePrepareUs=0;
    std::uint64_t totalTiles=0,visibleTiles=0;
    tiny2d::Vector2 camera{},player{};
    bool hitX=false,hitY=false;
};
struct NavigationFrameSample {
    unsigned requested=0,active=0;
    int hp=0,maxHP=0,guardTicks=0;
    float playerSpeed=0,enemySpeed=0;
    const char* phase="", *lastChoice="";
    std::uint64_t offerSerial=0,choiceSerial=0,fieldRebuilds=0,cacheReuses=0;
    std::size_t visited=0,expanded=0;
    int targetX=0,targetY=0;
    unsigned graphEvents=0,fieldEvents=0;
    double graphUs=0,fieldUs=0,enemyUpdateUs=0,enemyCollisionUs=0,upgradeUs=0;
};
struct CpuSample {
    FrameInfo info;
    Stats stats;
    double update=0,prepare=0,lightExtract=0,build=0,poll=0,total=0;
    std::size_t activeLights=0,droppedLights=0;
    tiny2d::Size lightSize{};
    std::uint64_t hdrBytes=0,previewBytes=0,shadowBytes=0;
    tiny2d::Size normalSize{};
    double normalPrepare=0; // CPU tile generation on the frame that first installs the material.
    std::uint64_t materialBytes=0, materialUpload=0;
    unsigned activeParticles=0, activeFields=0;
    std::uint64_t droppedParticles=0, validGpuSamples=0;
    double forceUpdateUs=0, snapshotPrepareUs=0;
    MapFrameSample map;
    NavigationFrameSample nav;
    tiny2d::QueryStatus status=tiny2d::QueryStatus::Off;
};
struct GpuSample {
    FrameInfo info;
    std::uint64_t received=0;
    tiny2d::QueryStatus status;
    Timer::Durations us;
};
struct History {
    static constexpr std::size_t capacity=120;
    std::deque<CpuSample> cpu;
    std::deque<GpuSample> gpu;
    template<class T> static void append(std::deque<T>& window,T sample) {
        if (window.size()==capacity) window.pop_front();
        window.push_back(std::move(sample));
    }
    void recordGpu(std::uint64_t currentEpoch,const GpuSample& sample,std::uint64_t currentRevision=0) {
        if (sample.info.epoch!=currentEpoch || sample.info.revision!=currentRevision || sample.status!=tiny2d::QueryStatus::Complete) return;
        // A reused query slot can retire before an older slot, even across polls.
        auto pos=gpu.end();
        if (!gpu.empty() && sample.info.id<gpu.back().info.id)
            pos=std::lower_bound(gpu.begin(),gpu.end(),sample.info.id,
                [](const GpuSample& item,std::uint64_t id){return item.info.id<id;});
        gpu.insert(pos,sample);
        if (gpu.size()>capacity) gpu.pop_front();
    }
};
}
