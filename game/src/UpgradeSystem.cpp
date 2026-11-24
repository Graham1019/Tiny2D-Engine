#include "tiny2d_game/UpgradeSystem.h"
#include "NavigationRandom.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace tiny2d_game {
namespace {
constexpr std::array<const char*, 4> names{"Fleet", "Slow", "Vitality", "Guard"};
constexpr std::array<int, 4> caps{4, 4, 3, 3};
}

const char* upgradeName(UpgradeId id) {
    return names.at(static_cast<unsigned>(id));
}

UpgradeSystem::UpgradeSystem(std::uint32_t seed)
    : rng_(UINT64_C(0x5550475241444500) ^ seed) {}

int UpgradeSystem::level(UpgradeId id) const {
    return levels_.at(static_cast<unsigned>(id));
}

bool UpgradeSystem::eligible(UpgradeId id) const {
    const auto i = static_cast<unsigned>(id);
    return i < levels_.size() && levels_[i] < caps[i];
}

bool UpgradeSystem::createOffer(std::uint64_t epoch) {
    if (offer_) return false;
    std::array<UpgradeId, 4> pool{};
    int valid = 0;
    for (auto id : upgradePool) {
        if (eligible(id)) pool[valid++] = id;
    }
    if (valid == 0) return false;
    if (serial_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("upgrade serial exhausted");

    UpgradeOffer next;
    next.epoch = epoch;
    next.serial = ++serial_;
    next.count = std::min(3, valid);
    for (int i = 0; i < next.count; ++i) {
        const auto pick = i + drawNavigationIndex(rng_, static_cast<unsigned>(valid - i));
        std::swap(pool[i], pool[pick]);
        next.choices[i] = pool[i];
    }
    offer_ = next;
    return true;
}

bool UpgradeSystem::choose(std::uint64_t epoch, std::uint64_t serial, UpgradeId id) {
    if (!offer_ || epoch != offer_->epoch || serial != offer_->serial || !eligible(id))
        return false;
    const auto end = offer_->choices.begin() + offer_->count;
    if (std::find(offer_->choices.begin(), end, id) == end) return false;
    ++levels_[static_cast<unsigned>(id)];
    lastChoice_ = UpgradeChoice{epoch, serial, id};
    offer_.reset();
    return true;
}

} // namespace tiny2d_game
