#include "tiny2d_game/ReplayBranch.h"
#include "tiny2d_game/SnapshotCodec.h"
#include <charconv>
#include <iostream>
#include <string_view>

using namespace tiny2d_game;

int main(int argc,char** argv) {
    try {
        if (argc!=3 && argc!=4)
            throw std::invalid_argument("usage: tiny2d_timeline_verify replay|branch FILE [CURSOR]");
        const std::string_view kind=argv[1];
        ReplayData tape;
        if (kind=="branch") {
            const auto branch=decodeBranch(readBoundedFile(argv[2],maxBranchBytes));
            const auto& m=branch.metadata();
            std::cout<<"branch_id="<<m.branchId<<" parent_id="<<m.parentId
                     <<" parent_recording_id="<<m.parentRecordingId<<" fork_cursor="<<m.forkCursor<<'\n';
            tape=branch.replay();
        } else if (kind=="replay") tape=decodeReplay(readBoundedFile(argv[2],maxReplayBytes));
        else throw std::invalid_argument("expected replay or branch");
        Timeline timeline(std::move(tape));
        auto cursor=timeline.data().events.size();
        if (argc==4) {
            const std::string_view value=argv[3];
            const auto end=value.data()+value.size();
            const auto parsed=std::from_chars(value.data(),end,cursor);
            if (parsed.ec!=std::errc{} || parsed.ptr!=end) throw std::invalid_argument("invalid cursor");
        }
        const auto seek=timeline.seek(cursor);
        const auto& s=timeline.session();
        std::cout<<"verified="<<timeline.data().events.size()<<" recording_id="<<timeline.recordingId()
                 <<" cursor="<<cursor<<" epoch="<<s.world().epoch()<<" tick="<<s.world().tick()
                 <<" phase="<<phaseName(s.phase())<<" manual_paused="<<s.world().paused()
                 <<" hp="<<s.hp()<<'/'<<s.upgrades().maxHP()<<" player_speed="<<s.upgrades().playerSpeed()
                 <<" checkpoint="<<seek.checkpointCursor<<" replayed="<<seek.replayedEvents
                 <<" hash="<<std::hex<<stateHash(s.captureSnapshot())<<'\n';
    } catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
