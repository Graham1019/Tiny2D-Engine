#pragma once
#include "tiny2d_game/ReplayBranch.h"
#include "tiny2d_game/Timeline.h"
#include <memory>
#include <optional>

namespace lab {

// Two independently owned histories. Only the tip of an explicitly forked child
// accepts production input. Inspecting a child creates an immutable view of it.
class TimelineView {
public:
    explicit TimelineView(tiny2d_game::ReplayData);
    explicit TimelineView(tiny2d_game::ReplayBranch);
    const tiny2d_game::NavigationSession& session() const;
    const tiny2d_game::ReplayData& data() const;
    const tiny2d_game::Timeline& index() const;
    const tiny2d_game::ReplayBranch* selectedBranch() const;
    std::size_t cursor() const;
    std::uint64_t branchId() const;
    bool editing() const { return surface_==Surface::ChildLive; }
    bool childSelected() const { return surface_!=Surface::Parent; }
    bool hasChild() const { return bool(child_); }
    std::shared_ptr<const tiny2d_game::ReplayBranch> child() const { return child_; }
    std::size_t checkpointBytes() const;
    std::size_t checkpointCount() const;
    void seek(std::size_t);
    bool next();
    void fork(std::uint64_t id);
    void inspectChild();
    void inspectParent();
    void resumeChild();
    bool append(const tiny2d_game::ReplayCommand&);
private:
    enum class Surface { Parent,ChildView,ChildLive };
    tiny2d_game::Timeline root_;
    std::optional<tiny2d_game::ReplayBranch> rootBranch_;
    std::shared_ptr<tiny2d_game::ReplayBranch> child_;
    std::unique_ptr<tiny2d_game::Timeline> childView_;
    Surface surface_=Surface::Parent;
};
}
