#include "SurvivorTrace.h"
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <system_error>

SurvivorCsv::SurvivorCsv(const std::filesystem::path& path):stream_(std::fopen(path.c_str(),"wx")) {
    if (!stream_) throw std::system_error(errno,std::generic_category(),"create new "+path.string());
}
SurvivorCsv::~SurvivorCsv() { if (stream_) std::fclose(stream_); }
void SurvivorCsv::line(const std::string& row) {
    if (std::fwrite(row.data(),1,row.size(),stream_)!=row.size() || std::fputc('\n',stream_)==EOF)
        throw std::runtime_error("write Survivor CSV");
}
void SurvivorCsv::finish() {
    auto* file=stream_;
    stream_=nullptr;
    if (std::fclose(file)) throw std::runtime_error("finish Survivor CSV");
}

const char* survivorTraceHeader() {
    return "tick,phase,hp,kills,spawned,fired,hits,enemies,projectiles,next_fire,next_spawn,vulnerable_at,"
           "entity_serial,event_sequence,entities,events,remaining,contacts";
}
namespace {
std::uint32_t bits(float f) {
    std::uint32_t u;
    static_assert(sizeof(u)==sizeof(f));
    std::memcpy(&u,&f,sizeof(u));
    return u;
}
const char* eventName(tiny2d_game::CombatEventKind kind) {
    using K=tiny2d_game::CombatEventKind;
    switch(kind) {
    case K::ProjectileHit: return "ProjectileHit";
    case K::EnemyKilled: return "EnemyKilled";
    case K::PlayerDamaged: return "PlayerDamaged";
    }
    throw std::logic_error("unknown combat event");
}
}
std::string survivorTraceRow(const tiny2d_game::GameSession& g,std::uint64_t previousEvent) {
    auto schedule=g.schedule();
    std::ostringstream row;
    row<<g.tickCount()<<','<<phaseName(g.phase())<<','<<g.playerHealth()<<','<<g.killCount()<<','
       <<g.spawnedCount()<<','<<g.firedCount()<<','<<g.hitCount()<<','<<g.enemyCount()<<','<<g.projectileCount()<<','
       <<schedule.nextFire<<','<<schedule.nextSpawn<<','<<schedule.vulnerableAt<<','<<schedule.nextEntitySerial<<','
       <<g.eventSequence()<<',';
    // Per-process Scene IDs do not survive restart. Slot/generation and the
    // simulation's stable entity serial fully identify an actor within its run.
    for (const auto& [id,entity]:g.entities()) {
        const auto& o=*g.scene().find(id);
        row<<id.index<<'.'<<id.generation<<'/'<<unsigned(entity.kind)<<'/'<<entity.serial<<'/'
           <<entity.health<<'/'<<entity.expires<<'/'<<std::hex<<bits(o.transform.position.x)<<'/'
           <<bits(o.transform.position.y)<<'/'<<bits(o.velocity.x)<<'/'<<bits(o.velocity.y)<<'/'
           <<bits(o.transform.scale.x)<<'/'<<bits(o.transform.scale.y)<<'/'<<bits(o.transform.rotation)<<std::dec<<';';
    }
    row<<',';
    for (auto i=previousEvent+1;i<=g.eventSequence();++i) {
        const auto* event=g.event(i);
        if (!event) throw std::logic_error("Survivor trace lost a combat event");
        row<<event->sequence<<'/'<<eventName(event->kind)<<'/'<<event->tick<<'/'<<std::hex
           <<bits(event->position.x)<<'/'<<bits(event->position.y)<<std::dec<<';';
    }
    row<<','<<g.remainingTicks()<<','<<g.scene().contacts();
    return row.str();
}
