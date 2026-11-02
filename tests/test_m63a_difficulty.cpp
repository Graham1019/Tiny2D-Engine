#include "GameSessionTestAccess.h"
#include "tiny2d_game/Difficulty.h"
using namespace tiny2d_game;
int main(){try{
    const unsigned spawn[]={90,75,65,57,50},minimum[]={36,30,26,23,20},fire[]={36,34,32,30,28};
    CombatConfig prior;
    std::cout<<"level,enemy_speed,initial_ticks,min_ticks,fire_ticks,projectile_speed\n";
    for(unsigned d=1;d<=5;++d){auto c=makeDifficultyConfig(difficultyLevel(d));c.validate();
        check(combatTicks(c.initialSpawnInterval)==spawn[d-1]&&combatTicks(c.minimumSpawnInterval)==minimum[d-1]&&combatTicks(c.fireInterval)==fire[d-1],"quantization table");
        check(c.playerMaxHealth==5&&c.playerSpeed==180&&c.enemyHealth==2&&c.projectileDamage==1&&c.invulnerability==.6&&c.projectileLifetime==2&&c.maxEnemies==48&&c.maxProjectiles==64&&c.initialEnemies==8&&c.spawnRampSeconds==90,"invariants");
        check(GameSession(c).durationTicks()==5400,"duration");
        if(d==1)check(c.enemySpeed==prior.enemySpeed&&c.projectileSpeed==prior.projectileSpeed&&c.initialSpawnInterval==prior.initialSpawnInterval&&c.minimumSpawnInterval==prior.minimumSpawnInterval&&c.fireInterval==prior.fireInterval,"default identity");
        else check(c.enemySpeed>prior.enemySpeed&&c.projectileSpeed>prior.projectileSpeed&&c.initialSpawnInterval<prior.initialSpawnInterval&&c.minimumSpawnInterval<prior.minimumSpawnInterval&&c.fireInterval<prior.fireInterval,"strict curves");
        std::cout<<d<<','<<c.enemySpeed<<','<<spawn[d-1]<<','<<minimum[d-1]<<','<<fire[d-1]<<','<<c.projectileSpeed<<'\n';prior=c;
    }
    for(unsigned n:{0u,6u,999u,~0u}){rejects([&]{difficultyLevel(n);});rejects([&]{makeDifficultyConfig(static_cast<DifficultyLevel>(n));});rejects([&]{difficultyName(static_cast<DifficultyLevel>(n));});}
}catch(const std::exception&e){std::cerr<<e.what();return 1;}}
