#include "NavigationApp.h"
#include <iostream>

namespace lab {
using namespace tiny2d_game;

NavigationApp::NavigationApp(const Options& options,const tiny2d::MapParameters& parameters)
    :session(options.seed,parameters,options.navEnemies),view{options.navField,options.navPath} {}

bool NavigationApp::playing() const { return session.phase()==NavigationPhase::Playing; }

void NavigationApp::recordInitialBuild(CpuSample& cpu) const {
    const auto& stats=session.navigator().stats();
    cpu.nav.graphEvents=1;
    cpu.nav.fieldEvents=1;
    cpu.nav.graphUs=stats.graphBuildUs;
    cpu.nav.fieldUs=stats.fieldBuildUs;
}

bool NavigationApp::applyChoice(UpgradeChoice choice,Controls& controls,CpuSample& cpu) {
    if (!session.choose(choice.epoch,choice.serial,choice.id)) return false;
    controls.clearElapsed();
    cpu.nav.upgradeUs+=session.timings().upgradeUpdateUs;
    std::cout<<"upgrade_choice="<<upgradeName(choice.id)<<" serial="<<choice.serial<<" epoch="<<choice.epoch
             <<" tick="<<session.world().tick()<<'\n';
    return true;
}

void NavigationApp::advance(Action action,const Options& options,Controls& controls,CpuSample& cpu) {
    const auto& world=session.world();
    if (options.frames) {
        if (const auto& offer=session.upgrades().offer()) {
            const auto id=offer->choices[options.navChooseLast?offer->count-1:0];
            if (!applyChoice({offer->epoch,offer->serial,id},controls,cpu))
                throw std::logic_error("finite navigation choice rejected");
        }
    }
    for (unsigned i=0;i<action.steps && playing();++i) {
        const auto input=options.navWalk?tiny2d::mapWalkInput(world.tick()):tiny2d::MapInput{action.motion.x,action.motion.y};
        const auto rebuilds=session.navigator().stats().fieldRebuildCount;
        const auto start=Clock::now();
        if (world.paused()) session.step(input);
        else session.fixedUpdate(input);
        cpu.update+=micros(start);
        cpu.map.collisionUs+=world.collisionUs();
        const auto& stats=session.navigator().stats();
        if (rebuilds!=stats.fieldRebuildCount) {
            ++cpu.nav.fieldEvents;
            cpu.nav.fieldUs+=stats.fieldBuildUs;
        }
        cpu.nav.enemyUpdateUs+=session.timings().enemyUpdateUs;
        cpu.nav.enemyCollisionUs+=session.timings().enemyCollisionUs;
        cpu.nav.upgradeUs+=session.timings().upgradeUpdateUs;
        if (!playing()) controls.clearElapsed();
    }
}

bool NavigationApp::drawOffer(bool finite,bool focused,Controls& controls,CpuSample& cpu) {
    if (!session.upgrades().offer()) return false;
    const auto choice=navigationOffer(session,finite);
    if (focused && choice) applyChoice(*choice,controls,cpu);
    return true;
}

void NavigationApp::prepare(Picture& picture,TileRange range,const Options& options,CpuSample& cpu) const {
    const auto& world=session.world();
    const auto& upgrades=session.upgrades();
    const auto& nav=session.navigator();
    const auto& stats=nav.stats();
    prepareNavigationEnemies(picture,world.map(),session.enemies(),session.enemyHalfSize(),range);
    prepareNavigationDebug(picture,session,range,view);
    prepareNavigationHud(picture,options.display,cpu.info.logical,session);
    auto& n=cpu.nav;
    n.requested=session.requestedEnemies();
    n.active=unsigned(session.enemies().size());
    n.phase=phaseName(session.phase());
    n.hp=session.hp();
    n.maxHP=upgrades.maxHP();
    n.playerSpeed=upgrades.playerSpeed();
    n.enemySpeed=upgrades.enemySpeed();
    n.guardTicks=upgrades.guardTicks();
    n.offerSerial=upgrades.serial();
    if (const auto& last=upgrades.lastChoice()) {
        n.choiceSerial=last->serial;
        n.lastChoice=upgradeName(last->id);
    }
    n.targetX=nav.field().target().x;
    n.targetY=nav.field().target().y;
    n.fieldRebuilds=stats.fieldRebuildCount;
    n.cacheReuses=stats.cacheReuseCount;
    n.visited=stats.visitedNodes;
    n.expanded=stats.expandedNodes;
}

void NavigationApp::printStatus() const {
    const auto& u=session.upgrades();
    std::cout<<" phase="<<phaseName(session.phase())<<" hp="<<session.hp()
             <<" enemies="<<session.enemies().size()<<" requested_enemies="<<session.requestedEnemies()
             <<" offer_serial="<<u.serial()<<" last_choice="<<(u.lastChoice()?upgradeName(u.lastChoice()->id):"none");
    if (*session.spawnShortfallReason()) std::cout<<" spawn_shortfall=\""<<session.spawnShortfallReason()<<'"';
}
}
