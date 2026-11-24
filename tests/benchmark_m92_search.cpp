// Compare the production shared field with an independent, early-stopping forward BFS.
// Both methods reuse a prebuilt collision-aware graph and see the same actor slots.
#include "tiny2d_game/NavigationSession.h"
#include "tiny2d/GridNavigator.h"
#include <chrono>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

using namespace tiny2d;
using namespace tiny2d_game;
using Clock = std::chrono::steady_clock;
using Hop = std::optional<TileCoord>;

namespace {
double elapsed(Clock::time_point start) {
    return std::chrono::duration<double,std::micro>(Clock::now()-start).count();
}

class ForwardSearch {
    int width_;
    std::vector<unsigned> seen_;
    std::vector<TileCoord> queue_, firstHop_;
    unsigned serial_ = 0;
public:
    std::size_t expanded = 0;
    explicit ForwardSearch(const TileMap& map)
        : width_(map.width()), seen_(map.capacity()), queue_(map.capacity()), firstHop_(map.capacity()) {}
    Hop find(const NavigationGrid& grid, TileCoord start, TileCoord target) {
        if (start == target) return {};
        ++serial_;
        auto index = [&](TileCoord c) { return c.y*width_+c.x; };
        seen_[index(start)] = serial_;
        queue_[0] = start;
        std::size_t head=0, tail=1;
        while (head < tail) {
            const auto cell=queue_[head++];
            ++expanded;
            for (std::size_t direction=0;direction<cardinalOffsets.size();++direction) {
                const auto offset=cardinalOffsets[direction];
                const TileCoord next{cell.x+offset.x,cell.y+offset.y};
                if (!grid.hasEdge(cell,direction)) continue;
                const auto i=index(next);
                if (seen_[i]==serial_) continue;
                seen_[i]=serial_;
                firstHop_[i] = cell==start ? next : firstHop_[index(cell)];
                if (next==target) return firstHop_[i];
                queue_[tail++]=next;
            }
        }
        return {};
    }
};

void trial(const MapParameters& params, int count, bool walk) {
    NavigationSession spawning(71,params,count);
    const auto& original=spawning.world();
    MapSession player=original;
    const auto& map=player.map();
    auto& graph=spawning.navigator().grid();
    ForwardSearch independent(map);
    GridNavigator shared;
    std::vector<TileCoord> starts;
    for (const auto& enemy : spawning.enemies()) starts.push_back(map.worldToTile(enemy.position));
    std::vector<Hop> answers(starts.size());
    for (int sample=0;sample<16;++sample) {
        for (int t=0;t<80;++t) player.step(walk ? mapWalkInput(player.tick()) : MapInput{});
        const auto target=map.worldToTile(player.playerPosition());
        const auto before=shared.stats().fieldRebuildCount;
        shared.refresh(map,player.epoch(),{5,5},target);
        const bool rebuilt=before!=shared.stats().fieldRebuildCount;
        // Graph preparation is common to both strategies. Time the BFS and read-only next-hop lookups.
        const auto lookupStart=Clock::now();
        for (std::size_t i=0;i<starts.size();++i) answers[i]=shared.next(starts[i]);
        const auto sharedUs=elapsed(lookupStart)+(rebuilt ? shared.stats().fieldBuildUs : 0.);
        const auto refStart=Clock::now();
        independent.expanded=0;
        std::size_t matches=0;
        for (std::size_t i=0;i<starts.size();++i)
            matches += independent.find(graph,starts[i],target)==answers[i];
        const auto refUs=elapsed(refStart);
        if (matches!=starts.size()) throw std::runtime_error("independent BFS next-hop mismatch");
        std::cout<<params.width<<','<<params.height<<','<<count<<','<<walk<<','<<player.tick()<<','
                 <<target.x<<','<<target.y<<','<<rebuilt<<','<<(rebuilt ? shared.stats().expandedNodes : 0)<<','
                 <<sharedUs<<','<<starts.size()<<','<<independent.expanded<<','<<refUs<<','<<matches<<'\n';
    }
}
}
int main() {
    std::cout<<"width,height,enemies,walk,tick,target_x,target_y,shared_builds,shared_expanded,shared_us,"
                 "forward_searches,forward_expanded,forward_us,matching_hops\n";
    for (auto params : {MapParameters{64,40},MapParameters{128,96}})
        for (auto count : {0,16,64,256})
            for (bool walk : {false,true}) trial(params,count,walk);
}
