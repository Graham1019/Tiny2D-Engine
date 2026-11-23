#pragma once
#include "Presentation.h"
#include "tiny2d/TimestampQueries.h"
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
    double update=0,prepare=0,build=0,poll=0,total=0;
    std::uint64_t hdrBytes=0,previewBytes=0;
    tiny2d::QueryStatus status=tiny2d::QueryStatus::Off;
};
struct GpuSample {
    FrameInfo info;
    std::uint64_t received=0;
    tiny2d::QueryStatus status;
    Timer::Durations us;
};
struct History {
    std::deque<CpuSample> cpu;
    std::deque<GpuSample> gpu;
    template<class T> static void append(std::deque<T>& window,T sample) {
        if (window.size()==120) window.pop_front();
        window.push_back(std::move(sample));
    }
};
}
