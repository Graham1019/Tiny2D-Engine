// This driver compiles against both public APIs, including the accepted M9.2 tree.
// Each line is a single boundary. Encode floats as binary32 hex and all tiles in row order.
#include "tiny2d_game/NavigationSession.h"
#include <cstring>
#include <iostream>
using namespace tiny2d_game;

void trace(const NavigationSession& s) {
    auto word=[](auto x){std::cout<<+x<<' ';};
    auto real=[&](float x){std::uint32_t bits; std::memcpy(&bits,&x,4); word(bits);};
    auto point=[&](tiny2d::Vector2 p){real(p.x);real(p.y);};
    auto cell=[&](tiny2d::TileCoord t){word(t.x);word(t.y);};
    const auto& w=s.world();
    const auto& p=w.parameters();
    word(w.seed()); word(p.width);word(p.height);real(p.tileSize);word(p.roomCount);word(p.extraConnections);
    cell(w.map().spawn());cell(w.map().exit());
    for (auto t:w.map().cells()) std::cout.put(t==tiny2d::Tile::Floor?'1':'0');
    std::cout.put(' ');
    word(w.epoch());word(w.tick());word(w.paused());point(w.playerPosition());
    word(w.collision().hitX);word(w.collision().hitY);word(w.collision().tilesQueried);
    word(static_cast<unsigned>(s.phase()));word(s.hp());word(s.vulnerableAt());word(s.nextOfferTick());
    word(s.requestedEnemies());word(s.spawnMinimumDistance());word(s.lastDamageSlot().value_or(999));
    word(s.spawnRngState());word(s.enemies().size());
    for (const auto& e:s.enemies()) {
        word(e.slot);word(e.epoch);point(e.position);word(e.waypoint.has_value());
        if (e.waypoint) cell(*e.waypoint);
    }
    const auto& u=s.upgrades();
    for (auto id:upgradePool) word(u.level(id));
    word(u.rngState());word(u.serial());word(u.offer().has_value());
    if (auto o=u.offer()) {
        word(o->epoch);word(o->serial);word(o->count);
        for (int i=0;i<o->count;++i) word(static_cast<unsigned>(o->choices[i]));
    }
    word(u.lastChoice().has_value());
    if (auto c=u.lastChoice()) {word(c->epoch);word(c->serial);word(static_cast<unsigned>(c->id));}
    std::cout.put('\n');
}
int main() {
    std::ios::sync_with_stdio(false);
    std::cout<<std::hex;
    for (int n:{0,16,256}) {
        NavigationSession s(71,{},n);
        trace(s);
        for (int map=0;map<2;++map) {
            while (s.phase()!=NavigationPhase::Victory && s.phase()!=NavigationPhase::GameOver) {
                if (auto o=s.upgrades().offer()) {
                    s.setPaused(true);trace(s);
                    if (!s.choose(o->epoch,o->serial,o->choices[o->count-1])) return 1;
                    trace(s);s.setPaused(false);trace(s);
                }
                s.fixedUpdate(tiny2d::mapWalkInput(s.world().tick()));
                trace(s);
            }
            s.reset();trace(s);
            s.regenerate(112,{21,17});trace(s);
        }
    }
}
