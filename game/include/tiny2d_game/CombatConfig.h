#pragma once
#include <cstddef>
namespace tiny2d_game {
struct CombatConfig {
    int playerMaxHealth=5, enemyHealth=2, projectileDamage=1;
    float playerSpeed=180, enemySpeed=65, projectileSpeed=420;
    double invulnerability=.6, fireInterval=.6, projectileLifetime=2;
    double initialSpawnInterval=1.5, minimumSpawnInterval=.6;
    double spawnRampSeconds=90;
    std::size_t initialEnemies=8, maxEnemies=48, maxProjectiles=64;
    void validate() const;
};
}
