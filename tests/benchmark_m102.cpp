// Common cursor sequence for every checkpoint policy on a given immutable tape.
#include "tiny2d_game/Timeline.h"
#include "tiny2d_game/SnapshotCodec.h"
#include <chrono>
#include <iostream>

using namespace tiny2d_game;
namespace {
ReplayData journey(int enemies, const tiny2d::MapParameters& p) {
    NavigationSession live(71,p,enemies);
    ReplayRecorder recorder(live);
    while (recorder.data().events.size()<2400) {
        ReplayCommand command=ReplayCommand::advance(tiny2d::mapWalkInput(live.world().tick()));
        if (live.phase()==NavigationPhase::AwaitingUpgrade) {
            const auto& offer=*live.upgrades().offer();
            command=ReplayCommand::choose({offer.epoch,offer.serial,offer.choices[0]});
        } else if (live.world().tick()==720 || live.phase()==NavigationPhase::GameOver)
            command=ReplayCommand::reset();
        if (!recorder.apply(live,command)) throw std::logic_error("unexpected fixture rejection");
    }
    return recorder.data();
}
void benchmark(int n, const tiny2d::MapParameters& p) {
    const auto data=journey(n,p);
    for (auto interval:{0u,60u,120u,240u,480u}) {
        TimelineOptions options;
        options.interval=interval;
        const auto start=std::chrono::steady_clock::now();
        Timeline timeline(data,options);
        const auto buildUs=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
        std::uint32_t rng=71;
        for (int sample=-3;sample<128;++sample) {
            rng=rng*1664525u+1013904223u;
            const auto target=static_cast<std::size_t>((std::uint64_t(rng)*(data.events.size()+1))>>32);
            const auto stats=timeline.seek(target);
            const auto expected=target?data.events[target-1].expectedHash:stateHash(data.base);
            if (stateHash(timeline.session().captureSnapshot())!=expected || timeline.cursor()!=target ||
                stats.checkpointCursor+stats.replayedEvents!=target)
                throw std::runtime_error("measured seek did not verify");
            if (sample<0) continue;
            std::cout<<p.width<<','<<p.height<<','<<n<<','<<data.events.size()<<','<<interval<<','<<sample<<','
                     <<target<<','<<stats.checkpointCursor<<','<<stats.replayedEvents<<','<<timeline.checkpoints().size()
                     <<','<<timeline.checkpointBytes()<<','<<timeline.maxGap()<<','<<timeline.worstReplay()<<','
                     <<buildUs<<','<<stats.restoreUs<<','<<stats.replayUs<<','<<stats.totalUs<<'\n';
        }
    }
}
}
int main() {
    try {
        std::cout<<"width,height,enemies,events,interval,sample,target,checkpoint,replayed,checkpoint_count,"
                   "checkpoint_bytes,max_gap,worst_replay,build_us,restore_us,replay_us,seek_us\n";
        benchmark(16,{64,40});
        benchmark(256,{128,96});
    } catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
