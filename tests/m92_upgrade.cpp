#include "M5Test.h"
#include "tiny2d_game/UpgradeSystem.h"

#include <algorithm>
#include <set>

using namespace tiny2d_game;

int main() {
    try {
        UpgradeSystem system(71), mirror(71);
        constexpr std::uint64_t epoch = 32;
        constexpr std::array<int,14> expectedFirst{2,0,3,2,0,3,3,0,1,0,1,1,1,2};
        bool sawTwo = false, sawOne = false;
        for (unsigned round = 0; round < expectedFirst.size(); ++round) {
            check(system.createOffer(epoch) && mirror.createOffer(epoch), "valid pool issues an offer");
            const auto offer = *system.offer();
            check(offer.count == mirror.offer()->count && offer.choices == mirror.offer()->choices &&
                  system.rngState() == mirror.rngState(), "equal-seed sequences match");
            check(offer.epoch == epoch && offer.serial == round+1 &&
                  static_cast<int>(offer.choices[0]) == expectedFirst[round], "pinned candidate order");
            if (round == 0) {
                check(offer.choices == std::array<UpgradeId,3>{UpgradeId::Vitality, UpgradeId::Slow, UpgradeId::Fleet}
                      && system.rngState() == UINT64_C(0x2ff6b47ebf23b986), "first draw matches pinned SplitMix64 stream");
            }
            std::set<UpgradeId> offered(offer.choices.begin(), offer.choices.begin()+offer.count);
            check(static_cast<int>(offered.size()) == offer.count, "draws are unique");
            int eligible = 0;
            for (auto id : upgradePool) eligible += system.eligible(id);
            check(offer.count == std::min(3,eligible), "correct number of choices after filtering caps");
            for (auto id : offered) check(system.eligible(id), "all choices eligible");

            const auto rng = system.rngState();
            check(!system.createOffer(epoch) && !system.choose(epoch+1,offer.serial,offer.choices[0]) &&
                  !system.choose(epoch,offer.serial-1,offer.choices[0]) &&
                  !system.choose(epoch,offer.serial,static_cast<UpgradeId>(255)), "reject stale or invalid events");
            for (auto id : upgradePool) {
                if (!offered.count(id)) check(!system.choose(epoch,offer.serial,id), "absent choice rejected");
                check(system.level(id) == mirror.level(id), "invalid requests leave levels unchanged");
            }
            check(system.rngState() == rng && system.offer()->choices == offer.choices &&
                  system.offer()->serial == offer.serial, "invalid requests preserve offer and RNG");
            const auto id = offer.choices[0];
            const auto before = system.level(id);
            check(system.choose(epoch,offer.serial,id) && mirror.choose(epoch,offer.serial,id) &&
                  !system.choose(epoch,offer.serial,id) && !system.offer() &&
                  system.level(id) == before+1 && system.rngState() == rng &&
                  system.lastChoice()->id == id && system.lastChoice()->epoch == epoch &&
                  system.lastChoice()->serial == offer.serial, "apply exactly once without drawing RNG");
            sawOne |= offer.count == 1;
            sawTwo |= offer.count == 2;
        }
        const auto rng = system.rngState();
        check(sawOne && sawTwo && !system.createOffer(epoch) && !system.offer() &&
              system.serial() == 14 && system.rngState() == rng &&
              system.playerSpeed() == 168 && system.enemySpeed() == 36 &&
              system.maxHP() == 8 && system.guardTicks() == 72, "capped pool skips without consuming RNG");
        std::cout << "14 golden choices, 4 caps, 3/2/1/0 eligible choices and atomic rejection passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
