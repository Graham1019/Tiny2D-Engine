#pragma once
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
};
using Timer=tiny2d::TimestampRing<FrameInfo,PassCount+1>;
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
    void recordGpu(std::uint64_t currentEpoch,const GpuSample& sample) {
        if (sample.info.epoch!=currentEpoch || sample.status!=tiny2d::QueryStatus::Complete) return;
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
