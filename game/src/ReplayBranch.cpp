#include "tiny2d_game/ReplayBranch.h"
#include "tiny2d_game/SnapshotCodec.h"
#include "BinaryCodec.h"

#include <utility>

namespace tiny2d_game {
namespace {
constexpr char magic[] = "T2BR";

void validateMetadata(const BranchMetadata& metadata) {
    if (!metadata.branchId || metadata.branchId == metadata.parentId || metadata.branchId == metadata.parentRecordingId)
        throw std::invalid_argument("branch ID must be nonzero and distinct from the parent identities");
    if (metadata.forkCursor > maxReplayEvents)
        throw std::invalid_argument("fork cursor exceeds parent event limit");
}
}

ReplayRecorder::ReplayRecorder(const NavigationSession& session, const ReplayPlayer& finished)
    : source_(&session) {
    if (finished.status() != PlaybackStatus::Finished)
        throw std::invalid_argument("resuming a branch requires a fully verified recording");
    if (!firstDifference(session.captureSnapshot(), finished.session().captureSnapshot()).empty())
        throw std::invalid_argument("branch session differs from the verified tip");
    data_ = finished.data();
}

struct ReplayBranch::State {
    explicit State(const NavigationSnapshot& base)
        : session(NavigationSession::fromSnapshot(base)), recorder(session) {}
    explicit State(const ReplayPlayer& verified)
        : session(NavigationSession::fromSnapshot(verified.session().captureSnapshot())), recorder(session, verified) {}
    NavigationSession session;
    ReplayRecorder recorder;
};

ReplayBranch::ReplayBranch(BranchMetadata metadata, std::unique_ptr<State> state)
    : metadata_(metadata), state_(std::move(state)) {}
ReplayBranch::ReplayBranch(ReplayBranch&&) noexcept = default;
ReplayBranch& ReplayBranch::operator=(ReplayBranch&&) noexcept = default;
ReplayBranch::~ReplayBranch() = default;

ReplayBranch ReplayBranch::fork(const Timeline& parent, std::uint64_t branchId,
                                std::optional<std::uint64_t> parentBranchId) {
    if (parentBranchId == 0) throw std::invalid_argument("parent branch ID must be nonzero");
    BranchMetadata metadata{branchId, parentBranchId.value_or(parent.recordingId()), parent.recordingId(), parent.cursor()};
    validateMetadata(metadata);
    return ReplayBranch(metadata, std::make_unique<State>(parent.session().captureSnapshot()));
}

const NavigationSession& ReplayBranch::session() const { return state_->session; }
const ReplayData& ReplayBranch::replay() const { return state_->recorder.data(); }
bool ReplayBranch::full() const { return state_->recorder.full(); }
bool ReplayBranch::apply(const ReplayCommand& command) {
    return state_->recorder.apply(state_->session, command);
}

std::vector<std::uint8_t> encodeBranch(const ReplayBranch& branch) {
    const auto tape = encodeReplay(branch.replay());
    constexpr std::size_t overhead = binary::headerBytes + 4 * sizeof(std::uint64_t) + sizeof(std::uint32_t);
    if (tape.size() > maxBranchBytes - overhead) throw std::length_error("branch exceeds byte budget");
    const auto& metadata = branch.metadata();
    binary::Writer payload;
    payload.bytes.reserve(4 * 8 + 4 + tape.size());
    payload.u64(metadata.branchId);
    payload.u64(metadata.parentId);
    payload.u64(metadata.parentRecordingId);
    payload.u64(metadata.forkCursor);
    payload.u32(static_cast<std::uint32_t>(tape.size()));
    payload.bytes.insert(payload.bytes.end(), tape.begin(), tape.end());
    return binary::wrap(magic, branchFormatVersion, payload.bytes);
}

ReplayBranch decodeBranch(const std::vector<std::uint8_t>& bytes) {
    auto in = binary::unwrap(bytes, magic, branchFormatVersion, maxBranchBytes);
    const BranchMetadata metadata{in.u64(), in.u64(), in.u64(), in.u64()};
    validateMetadata(metadata);
    const auto length = in.u32();
    if (length > maxReplayBytes || length != in.remaining())
        throw std::invalid_argument("embedded branch replay length mismatch");
    const auto* tape = in.take(length);
    ReplayPlayer verifier(decodeReplay(binary::Bytes(tape, tape + length)));
    while (verifier.next()) {}
    if (verifier.status() != PlaybackStatus::Finished)
        throw std::invalid_argument("invalid branch replay: " + verifier.error());
    return ReplayBranch(metadata, std::make_unique<ReplayBranch::State>(verifier));
}
} // namespace tiny2d_game
