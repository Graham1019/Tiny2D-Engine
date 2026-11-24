#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace tiny2d_game {

enum class UpgradeId : std::uint8_t { Fleet = 0, Slow = 1, Vitality = 2, Guard = 3 };
inline constexpr std::array<UpgradeId, 4> upgradePool{
    UpgradeId::Fleet, UpgradeId::Slow, UpgradeId::Vitality, UpgradeId::Guard};
const char* upgradeName(UpgradeId id);

struct UpgradeOffer {
    std::uint64_t epoch = 0, serial = 0;
    std::array<UpgradeId, 3> choices{};
    int count = 0;
};

struct UpgradeChoice {
    std::uint64_t epoch, serial;
    UpgradeId id;
};

struct UpgradeSnapshot;

class UpgradeSystem {
public:
    explicit UpgradeSystem(std::uint32_t seed);
    bool createOffer(std::uint64_t epoch);
    bool choose(std::uint64_t epoch, std::uint64_t serial, UpgradeId id);

    int level(UpgradeId id) const;
    static int levelCap(UpgradeId id);
    bool eligible(UpgradeId id) const;
    float playerSpeed() const { return 120.f + 12.f * level(UpgradeId::Fleet); }
    float enemySpeed() const { return 60.f - 6.f * level(UpgradeId::Slow); }
    int maxHP() const { return 5 + level(UpgradeId::Vitality); }
    int guardTicks() const { return 36 + 12 * level(UpgradeId::Guard); }

    const std::optional<UpgradeOffer>& offer() const { return offer_; }
    const std::optional<UpgradeChoice>& lastChoice() const { return lastChoice_; }
    std::uint64_t serial() const { return serial_; }
    std::uint64_t rngState() const { return rng_; }

private:
    friend class NavigationSession;
    explicit UpgradeSystem(const UpgradeSnapshot& validated);
    void validateSeededChoices(std::uint32_t seed) const;
    std::array<int, 4> levels_{};
    std::uint64_t rng_, serial_ = 0;
    std::optional<UpgradeOffer> offer_;
    std::optional<UpgradeChoice> lastChoice_;
};

} // namespace tiny2d_game
