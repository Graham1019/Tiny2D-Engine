#include "tiny2d_game/GameSession.h"
#include "tiny2d_game/Difficulty.h"
#include "tiny2d/FixedTimestep.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <set>
using namespace tiny2d;
namespace tiny2d_game {
namespace {
Vector2 direction(Vector2 from, Vector2 to, float speed) {
    const double x=double(to.x)-from.x, y=double(to.y)-from.y;
    const double length=std::hypot(x,y);
    return length>0 ? Vector2{float(x/length*speed),float(y/length*speed)} : Vector2{};
}
}
void CombatConfig::validate() const {
    if(playerMaxHealth<=0||enemyHealth<=0||projectileDamage<=0)
        throw std::invalid_argument("health and damage must be positive");
    for(double speed:{double(playerSpeed),double(enemySpeed),double(projectileSpeed)})
        if(!std::isfinite(speed)||speed<0||speed>100000)
            throw std::invalid_argument("speed outside supported range");
    for(double time:{invulnerability,fireInterval,projectileLifetime,initialSpawnInterval,minimumSpawnInterval,spawnRampSeconds})
        if(!std::isfinite(time)||time<FixedTimestep::step||time>86400)
            throw std::invalid_argument("time must be between one tick and one day");
    if(minimumSpawnInterval>initialSpawnInterval||maxEnemies==0||maxEnemies>256||
       maxProjectiles==0||maxProjectiles>256||initialEnemies>maxEnemies)
        throw std::invalid_argument("invalid spawn or capacity configuration");
}
GameSession::GameSession(const CombatConfig& config, const SurvivalRules& rules):config_(config),rules_(rules) {
    rules_.validate();
    config_.validate();
    player_=create(EntityKind::Player,{0,0});
    for(std::size_t i=0;i<config_.initialEnemies;++i)spawnEnemy();
    nextFire_=combatTicks(config_.fireInterval);
    nextSpawn_=combatTicks(config_.initialSpawnInterval);
}
ObjectId GameSession::create(EntityKind kind, Vector2 p) {
    const float radius=kind==EntityKind::PlayerProjectile?4:12;
    auto id=scene_.create();
    try {
        auto& o=*scene_.find(id);
        o.transform.position={p.x-radius,p.y-radius};
        o.collider=Circle{{radius,radius},radius};
        o.sprite=SpriteData{0,{radius*2,radius*2},1,.2f,.1f,1,0};
        if(kind==EntityKind::Player) { o.sprite->r=.15f; o.sprite->g=.5f; o.sprite->b=1; }
        if(kind==EntityKind::PlayerProjectile) { o.sprite->g=.9f; o.sprite->b=.1f; o.sprite->renderOrder=1; }
        entities_.emplace(id,EntityState{kind,serial_++,kind==EntityKind::Player?config_.playerMaxHealth:
            kind==EntityKind::Enemy?config_.enemyHealth:1,0});
    } catch(...) { scene_.destroy(id); throw; }
    return id;
}
Vector2 GameSession::center(ObjectId id) const {
    const auto& o=*scene_.find(id);
    return o.transform.position+std::get<Circle>(*o.collider).center;
}
uint64_t GameSession::enemyCount() const {
    return std::count_if(entities_.begin(),entities_.end(),[](const auto& e){return e.second.kind==EntityKind::Enemy;});
}
uint64_t GameSession::projectileCount() const {
    return std::count_if(entities_.begin(),entities_.end(),[](const auto& e){return e.second.kind==EntityKind::PlayerProjectile;});
}
double GameSession::elapsedSimulationTime() const { return tick_*FixedTimestep::step; }
void GameSession::spawnEnemy() {
    // Fixed normalized direction table avoids distribution/platform dependencies.
    static const Vector2 directions[]={{1,0},{.6f,.8f},{0,1},{-.8f,.6f},{-1,0},{-.6f,-.8f},{0,-1},{.8f,-.6f}};
    auto d=directions[spawned_%8];float radius=320+float(spawned_%4)*40;
    auto p=center(player_);create(EntityKind::Enemy,{p.x+d.x*radius,p.y+d.y*radius});++spawned_;
}
void GameSession::fire() {
    if(projectileCount()>=config_.maxProjectiles)return;
    auto p=center(player_);ObjectId target;double best=std::numeric_limits<double>::infinity();
    for(const auto& [id,state]:entities_) if(state.kind==EntityKind::Enemy&&state.health>0&&scene_.find(id)) {
        auto q=center(id);double x=double(q.x)-p.x,y=double(q.y)-p.y,d=x*x+y*y;
        if(d<best) {best=d;target=id;} // map order resolves equal distances by full ObjectId
    }
    if(!scene_.find(target))return;
    auto velocity=direction(p,center(target),config_.projectileSpeed);
    if(best==0)velocity={config_.projectileSpeed,0}; // deterministic +X fallback
    auto id=create(EntityKind::PlayerProjectile,p);
    scene_.find(id)->velocity=velocity;
    entities_.at(id).expires=tick_+combatTicks(config_.projectileLifetime);
    ++fired_;
}
void GameSession::erase(ObjectId id) { scene_.destroy(id);entities_.erase(id); }
void GameSession::fixedUpdate(const GameplayInput& input) {
    if(!isPlaying())return; // Terminal calls ignore even invalid input.
    if(!std::isfinite(input.moveX)||!std::isfinite(input.moveY))
        throw std::invalid_argument("non-finite gameplay input");
    if(tick_==UINT64_MAX)throw std::overflow_error("simulation tick exhausted");
    ++tick_;
    // Expiry is exclusive: expired projectiles do not move or hit this tick.
    std::vector<ObjectId> expired;
    for(const auto& [id,s]:entities_)if(s.kind==EntityKind::PlayerProjectile&&tick_>=s.expires)expired.push_back(id);
    for(auto id:expired)erase(id);
    auto p=center(player_);
    scene_.find(player_)->velocity=playerAlive()?direction({}, {input.moveX,input.moveY},config_.playerSpeed):Vector2{};
    if(tick_>=nextFire_) {nextFire_=tick_+combatTicks(config_.fireInterval);if(playerAlive())fire();}
    if(tick_>=nextSpawn_) {
        double fraction=std::min(1.0,elapsedSimulationTime()/config_.spawnRampSeconds);
        nextSpawn_=tick_+combatTicks(config_.initialSpawnInterval+(config_.minimumSpawnInterval-config_.initialSpawnInterval)*fraction);
        if(enemyCount()<config_.maxEnemies)spawnEnemy();
    }
    // All objects created above participate in this Scene snapshot, including new enemies.
    for(const auto& [id,s]:entities_)if(s.kind==EntityKind::Enemy)
        scene_.find(id)->velocity=playerAlive()?direction(center(id),p,config_.enemySpeed):Vector2{};
    std::vector<CollisionEvent> events;
    scene_.update(FixedTimestep::step,{},[&](Scene&,const CollisionEvent& e){
        if(e.phase!=CollisionPhase::Exit)events.push_back(e);
    });
    std::set<ObjectId> consumed;
    for(auto e:events) {
        auto a=entities_.find(e.a),b=entities_.find(e.b);
        if(a==entities_.end()||b==entities_.end())continue;
        if(a->second.kind==EntityKind::Enemy)std::swap(a,b);
        if(a->second.kind!=EntityKind::PlayerProjectile||b->second.kind!=EntityKind::Enemy||
           b->second.health<=0||consumed.count(a->first))continue;
        b->second.health=std::max(0,b->second.health-config_.projectileDamage);
        consumed.insert(a->first);++hits_;emit(CombatEventKind::ProjectileHit,center(a->first));
    }
    // Projectile kills win over contact damage within the same tick.
    if(playerAlive()&&tick_>=vulnerableAt_)for(auto e:events) {
        ObjectId other;
        if(e.a==player_)other=e.b;else if(e.b==player_)other=e.a;else continue;
        auto i=entities_.find(other);
        if(i!=entities_.end()&&i->second.kind==EntityKind::Enemy&&i->second.health>0) {
            --entities_.at(player_).health;emit(CombatEventKind::PlayerDamaged,center(player_));vulnerableAt_=tick_+combatTicks(config_.invulnerability);break;
        }
    }
    for(const auto& [id,s]:entities_)if(s.kind==EntityKind::Enemy&&s.health==0){consumed.insert(id);++kills_;emit(CombatEventKind::EnemyKilled,center(id));}
    for(auto id:consumed)erase(id);
    auto& sprite=*scene_.find(player_)->sprite;
    sprite.r=!playerAlive()?.3f:tick_<vulnerableAt_?1.f:.15f;
    sprite.g=!playerAlive()?.3f:tick_<vulnerableAt_?.9f:.5f;
    sprite.b=tick_<vulnerableAt_ && (tick_/4)%2 ? .35f:1.f;
    if(!playerAlive()) {scene_.find(player_)->velocity={};phase_=GamePhase::GameOver;}
    else if(tick_>=rules_.durationTicks)phase_=GamePhase::Victory;
}
}
