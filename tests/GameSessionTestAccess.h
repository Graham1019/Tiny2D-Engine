#pragma once
#include "tiny2d_game/GameSession.h"
#include "M5Test.h"
#include <cmath>
namespace tiny2d_game {
struct GameSessionTestAccess {
    static tiny2d::ObjectId enemy(GameSession& g,tiny2d::Vector2 p) {return g.create(EntityKind::Enemy,p);}
    static tiny2d::ObjectId shot(GameSession& g,tiny2d::Vector2 p,uint64_t lifetime=120) {
        auto id=g.create(EntityKind::PlayerProjectile,p);g.entities_.at(id).expires=g.tick_+lifetime;return id;
    }
    static void erase(GameSession& g,tiny2d::ObjectId id){g.erase(id);}
    static void fire(GameSession& g){g.fire();}
    static auto center(const GameSession& g,tiny2d::ObjectId id){return g.center(id);}
};
inline CombatConfig quiet() {
    CombatConfig c;c.initialEnemies=0;c.enemySpeed=0;c.initialSpawnInterval=c.minimumSpawnInterval=86400;c.fireInterval=86400;return c;
}
inline void step(GameSession& g,int n){for(int i=0;i<n;++i)g.fixedUpdate();}
}
