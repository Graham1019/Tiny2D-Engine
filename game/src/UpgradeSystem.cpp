#include "tiny2d_game/UpgradeSystem.h"
#include "NavigationRandom.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace tiny2d_game {
namespace {
constexpr std::array<const char*, 4> names{"Fleet", "Slow", "Vitality", "Guard"};
constexpr std::array<int, 4> caps{4, 4, 3, 3};

UpgradeOffer drawOffer(const std::array<int, 4>& levels, std::uint64_t epoch,
                       std::uint64_t serial, std::uint64_t& stream) {
    std::array<UpgradeId, 4> pool{};
    int valid = 0;
    for (auto id : upgradePool) {
        const auto index = static_cast<unsigned>(id);
        if (levels[index] < caps[index]) pool[valid++] = id;
    }
    UpgradeOffer next;
    next.epoch = epoch;
    next.serial = serial;
    next.count = std::min(3, valid);
    for (int i = 0; i < next.count; ++i) {
        const auto pick = i + drawNavigationIndex(stream, static_cast<unsigned>(valid - i));
        std::swap(pool[i], pool[pick]);
        next.choices[i] = pool[i];
    }
    return next;
}

// Within NavigationSession's five-offer budget, each serial consumes exactly
// three words. Validate offer witnesses locally, without issuing a gameplay
// command, mutating the stream, stepping the world, or substituting new choices.
UpgradeOffer witness(const std::array<int, 4>& levels, std::uint32_t seed, std::uint64_t serial) {
    auto stream = initialUpgradeRng(seed) + 3 * (serial - 1) * navigationRngIncrement;
    return drawOffer(levels, 0, serial, stream);
}

bool reachableLevels(const std::array<int, 4>& levels, std::uint32_t seed, std::uint64_t serial,
                     std::optional<UpgradeId> requiredLast) {
    if (serial == 0) return true;
    // At most five levels have been chosen. Try predecessor level vectors,
    // restricted by the recorded last ID and the seeded candidates at that serial.
    for (auto id : upgradePool) {
        const auto index = static_cast<unsigned>(id);
        if (!levels[index] || (requiredLast && id != *requiredLast)) continue;
        auto predecessor = levels;
        --predecessor[index];
        const auto o = witness(predecessor, seed, serial);
        const auto end = o.choices.begin() + o.count;
        if (std::find(o.choices.begin(), end, id) != end &&
            reachableLevels(predecessor, seed, serial - 1, std::nullopt))
            return true;
    }
    return false;
}
}

const char* upgradeName(UpgradeId id) {
    return names.at(static_cast<unsigned>(id));
}

UpgradeSystem::UpgradeSystem(std::uint32_t seed)
    : rng_(initialUpgradeRng(seed)) {}

int UpgradeSystem::levelCap(UpgradeId id) {
    return caps.at(static_cast<unsigned>(id));
}

int UpgradeSystem::level(UpgradeId id) const {
    return levels_.at(static_cast<unsigned>(id));
}

bool UpgradeSystem::eligible(UpgradeId id) const {
    const auto i = static_cast<unsigned>(id);
    return i < levels_.size() && levels_[i] < caps[i];
}

bool UpgradeSystem::createOffer(std::uint64_t epoch) {
    if (offer_) return false;
    // Detect exhaustion before advancing either the serial or the live stream.
    auto stream = rng_;
    const auto next = drawOffer(levels_, epoch, serial_ + 1, stream);
    if (!next.count) return false;
    if (serial_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("upgrade serial exhausted");
    ++serial_;
    rng_ = stream;
    offer_ = next;
    return true;
}

void UpgradeSystem::validateSeededChoices(std::uint32_t seed) const {
    if (offer_) {
        const auto expected = witness(levels_, seed, serial_);
        if (expected.count != offer_->count || expected.choices != offer_->choices)
            throw std::invalid_argument("pending offer choices differ from seeded order");
    }
    if (lastChoice_ && !reachableLevels(levels_, seed, lastChoice_->serial, lastChoice_->id))
        throw std::invalid_argument("upgrade levels or lastChoice impossible for seeded offers");
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
