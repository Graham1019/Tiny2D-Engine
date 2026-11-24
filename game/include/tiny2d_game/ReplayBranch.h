#pragma once

#include "tiny2d_game/Timeline.h"

#include <memory>

namespace tiny2d_game {

inline constexpr std::uint32_t branchFormatVersion = 1;
inline constexpr std::size_t maxBranchBytes = maxReplayBytes;

struct BranchMetadata {
    std::uint64_t branchId, parentId, parentRecordingId, forkCursor;
};

// The full child tape starts at the fork checkpoint. Parent future events are
// never borrowed, truncated, or modified. Moving a branch preserves the address
// of its recorded session. The caller assigns a distinct, nonzero branch ID.
class ReplayBranch {
public:
    static ReplayBranch fork(const Timeline& parent, std::uint64_t branchId,
                             std::optional<std::uint64_t> parentBranchId = std::nullopt);
    ReplayBranch(ReplayBranch&&) noexcept;
    ReplayBranch& operator=(ReplayBranch&&) noexcept;
    ~ReplayBranch();

    const BranchMetadata& metadata() const { return metadata_; }
    const NavigationSession& session() const;
    const ReplayData& replay() const;
    bool full() const;
    bool apply(const ReplayCommand& command);

private:
    friend ReplayBranch decodeBranch(const std::vector<std::uint8_t>&);
    struct State;
    ReplayBranch(BranchMetadata metadata, std::unique_ptr<State> state);
    BranchMetadata metadata_;
    std::unique_ptr<State> state_;
};

// T2BR carries provenance and a complete, independently playable T2RP v1 tape.
// Decode verifies its entire tape and creates a new child at the verified tip.
std::vector<std::uint8_t> encodeBranch(const ReplayBranch& branch);
ReplayBranch decodeBranch(const std::vector<std::uint8_t>& bytes);

} // namespace tiny2d_game
