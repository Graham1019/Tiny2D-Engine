// Header-only CPU probe. Build separately against each checkout (commands in history-cost.log).
#include "Observation.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>

void measure(bool late) {
    constexpr unsigned batch=1024, warmup=64, measured=512;
    lab::History history;
    std::array<lab::GpuSample,batch> arrivals{};
    std::array<double,measured> ns{};
    std::uint64_t checksum=0;
    for (auto& g:arrivals) {
        g.info.epoch=7;
        g.status=tiny2d::QueryStatus::Complete;
        g.us.fill(1.0);
    }
    for (unsigned block=0;block<warmup+measured;++block) {
        for (unsigned i=0;i<batch;++i) {
            // Swap the last two arrivals of every 32, as when a reused slot retires first.
            unsigned order=(late && i%32>=30) ? (i^1u) : i;
            auto& g=arrivals[i];
            g.info.id=1+block*batch+order;
            g.info.tick=g.info.id;
            g.received=2+block*batch+i;
        }
        auto start=std::chrono::steady_clock::now();
        for (const auto& g:arrivals) {
#ifdef M72_BEFORE
            if (g.info.epoch==7 && g.status==tiny2d::QueryStatus::Complete)
                lab::History::append(history.gpu,g);
#else
            history.recordGpu(7,g);
#endif
        }
        auto elapsed=std::chrono::steady_clock::now()-start;
        if (block>=warmup) ns[block-warmup]=std::chrono::duration<double,std::nano>(elapsed).count()/batch;
        checksum+=history.gpu.back().info.id;
    }
    std::sort(ns.begin(),ns.end());
    auto q=[&](double p){return ns[static_cast<unsigned>(std::ceil(p*measured))-1];};
    std::cout<<std::fixed<<std::setprecision(2)<<(late?"one_late_per_32":"ordered")
             <<" ns/record p50="<<q(.50)<<" p95="<<q(.95)<<" p99="<<q(.99)
             <<" blocks="<<measured<<" records/block="<<batch<<" retained="<<history.gpu.size()
             <<" latest_checksum="<<checksum<<'\n';
}
int main() { measure(false); measure(true); }
