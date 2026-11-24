// Release CPU benchmark. stdout is raw CSV. File I/O uses a new sibling name per sample.
#include "tiny2d_game/Replay.h"
#include "tiny2d_game/SnapshotCodec.h"
#include <chrono>
#include <cstdlib>
#include <iostream>

using namespace tiny2d_game;
namespace {
using Clock=std::chrono::steady_clock;
template<class F> double measure(F&& f) {
    const auto begin=Clock::now();
    f();
    return std::chrono::duration<double,std::micro>(Clock::now()-begin).count();
}
void stepOrChoose(NavigationSession& s) {
    if (auto offer=s.upgrades().offer()) s.choose(offer->epoch,offer->serial,offer->choices[0]);
    else s.fixedUpdate(tiny2d::mapWalkInput(s.world().tick()));
}
void sample(const NavigationSession& s, const std::filesystem::path& dir, const char* label) {
    for (int i=-2;i<120;++i) {
        NavigationSnapshot captured,decoded;
        std::vector<std::uint8_t> encoded,read;
        std::uint64_t hash;
        RestoreTimings timings;
        const auto capture=measure([&]{captured=s.captureSnapshot();});
        const auto encode=measure([&]{encoded=encodeSnapshot(captured);});
        const auto decode=measure([&]{decoded=decodeSnapshot(encoded);});
        std::optional<NavigationSession> candidate;
        const auto restore=measure([&]{candidate.emplace(NavigationSession::fromSnapshot(decoded,&timings));});
        const auto hashUs=measure([&]{hash=stateHash(captured);});
        const auto file=dir/"sample.snapshot";
        const auto write=measure([&]{writeNewFile(file,encoded);});
        const auto load=measure([&]{read=readBoundedFile(file,maxSnapshotBytes);});
        if (!std::filesystem::remove(file) || read!=encoded || hash!=stateHash(candidate->captureSnapshot()) ||
            !firstDifference(captured,candidate->captureSnapshot()).empty())
            throw std::runtime_error("benchmark roundtrip mismatch");

        // Paired production command sequences on equal initial sessions. Include
        // the state hash and append in recording time. Exclude base restore/setup.
        auto plain=*candidate, live=*candidate;
        ReplayRecorder recorder(live);
        std::vector<ReplayCommand> commands;
        if (auto offer=s.upgrades().offer()) commands.push_back(ReplayCommand::choose({offer->epoch,offer->serial,offer->choices[0]}));
        for (unsigned j=0;j<32;++j) {
            if (s.phase()==NavigationPhase::GameOver || s.phase()==NavigationPhase::Victory)
                commands.push_back(ReplayCommand::setPaused(j%2==0));
            else commands.push_back(ReplayCommand::advance(tiny2d::mapWalkInput(s.world().tick()+j)));
        }
        std::size_t accepted=0;
        auto plainRun=[&]{for (const auto& c:commands) accepted+=dispatchEvent(plain,c);};
        auto recordRun=[&]{for (const auto& c:commands) recorder.apply(live,c);};
        double base,record;
        if (i%2) {base=measure(plainRun);record=measure(recordRun);}
        else {record=measure(recordRun);base=measure(plainRun);}
        const auto bytes=encodeReplay(recorder.data());
        ReplayPlayer player(decodeReplay(bytes));
        std::size_t replayed=0;
        const auto verify=measure([&]{while (player.next()) ++replayed;});
        if (!accepted || replayed!=accepted || player.verifiedCount()!=accepted ||
            recorder.data().events.size()!=accepted || player.status()!=PlaybackStatus::Finished ||
            !firstDifference(plain.captureSnapshot(),player.session().captureSnapshot()).empty())
            throw std::runtime_error("benchmark replay mismatch");
        if (i<0) continue;
        std::cout<<s.world().map().width()<<','<<s.world().map().height()<<','<<s.enemies().size()<<','
                 <<label<<','<<s.world().tick()<<','<<i<<','<<encoded.size()<<','<<bytes.size()<<','
                 <<accepted<<','<<replayed<<','<<player.verifiedCount()<<','<<capture<<','<<encode<<','
                 <<decode<<','<<timings.semanticValidationUs<<','<<timings.cacheRebuildUs<<','<<restore<<','
                 <<hashUs<<','<<write<<','<<load<<','<<base/accepted<<','<<record/accepted<<','
                 <<(record-base)/accepted<<','<<verify/accepted<<'\n';
    }
}
}
int main(int argc,char** argv) {
    try {
        if (argc!=2) throw std::runtime_error("usage: tiny2d_m101_benchmark EMPTY_IO_DIRECTORY");
        std::filesystem::path dir=argv[1];
        if (!std::filesystem::is_directory(dir) || !std::filesystem::is_empty(dir))
            throw std::runtime_error("benchmark needs an empty scratch directory");
        std::cout<<"width,height,enemies,boundary,tick,sample,snapshot_bytes,replay_bytes,recorded,replayed,verified,"
                   "capture_us,encode_us,decode_us,semantic_us,cache_us,restore_us,hash_us,file_write_us,file_read_us,"
                   "plain_us_per_event,record_us_per_event,extra_us_per_event,verify_us_per_event\n";
        for (auto p:{tiny2d::MapParameters{64,40},tiny2d::MapParameters{128,96}}) {
            for (int n:{0,16,256}) {
                if (p.width==128 && n!=256) continue;
                NavigationSession s(71,p,n);
                sample(s,dir,"initial");
                while (s.world().tick()<37) stepOrChoose(s);
                sample(s,dir,"motion");
                bool sampledOffer=false;
                while (s.phase()!=NavigationPhase::GameOver && s.phase()!=NavigationPhase::Victory) {
                    if (!sampledOffer && s.phase()==NavigationPhase::AwaitingUpgrade) {
                        sample(s,dir,"offer");
                        sampledOffer=true;
                    }
                    stepOrChoose(s);
                }
                sample(s,dir,phaseName(s.phase()));
            }
        }
    } catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
