#include "TimelineView.h"

namespace lab {
using namespace tiny2d_game;
namespace {
// The two resident timelines together stay within the default Lab cache limit.
TimelineOptions options() {
    TimelineOptions o;
    o.byteBudget/=2;
    o.maxCheckpoints/=2;
    return o;
}
}

TimelineView::TimelineView(ReplayData data):root_(std::move(data),options()) {}
TimelineView::TimelineView(ReplayBranch branch):root_(branch.replay(),options()),rootBranch_(std::move(branch)) {}

const Timeline& TimelineView::index() const {
    if (surface_==Surface::ChildView) return *childView_;
    return root_;
}
const NavigationSession& TimelineView::session() const {
    return editing()?child_->session():index().session();
}
const ReplayData& TimelineView::data() const {
    return childSelected()?child_->replay():root_.data();
}
const ReplayBranch* TimelineView::selectedBranch() const {
    if (childSelected()) return &*child_;
    return rootBranch_?&*rootBranch_:nullptr;
}
std::size_t TimelineView::cursor() const { return editing()?data().events.size():index().cursor(); }
std::uint64_t TimelineView::branchId() const {
    auto branch=selectedBranch();
    return branch?branch->metadata().branchId:root_.recordingId();
}
std::size_t TimelineView::checkpointBytes() const {
    return root_.checkpointBytes()+(childView_?childView_->checkpointBytes():0);
}
std::size_t TimelineView::checkpointCount() const {
    return root_.checkpoints().size()+(childView_?childView_->checkpoints().size():0);
}
void TimelineView::inspectChild() {
    if (!child_) throw std::logic_error("no child to inspect");
    if (!childView_) {
        auto view=std::make_unique<Timeline>(child_->replay(),options());
        view->seek(child_->replay().events.size());
        childView_=std::move(view);
    }
    surface_=Surface::ChildView;
}
void TimelineView::inspectParent() { surface_=Surface::Parent; }
void TimelineView::seek(std::size_t cursor) {
    if (cursor>data().events.size()) throw std::out_of_range("event cursor exceeds recording length");
    if (editing()) {
        if (!childView_) {
            auto candidate=std::make_unique<Timeline>(child_->replay(),options());
            candidate->seek(cursor);
            childView_=std::move(candidate);
        } else childView_->seek(cursor);
        surface_=Surface::ChildView;
    } else (childSelected()?*childView_:root_).seek(cursor);
}
bool TimelineView::next() {
    if (editing()) return false;
    return (childSelected()?*childView_:root_).next();
}
void TimelineView::fork(std::uint64_t id) {
    if (hasChild() || childSelected()) throw std::logic_error("a child is already open");
    const auto parent=rootBranch_?std::optional{rootBranch_->metadata().branchId}:std::nullopt;
    child_=std::make_shared<ReplayBranch>(ReplayBranch::fork(root_,id,parent));
    surface_=Surface::ChildLive;
}
void TimelineView::resumeChild() {
    if (surface_!=Surface::ChildView || cursor()!=data().events.size())
        throw std::logic_error("resume at the child tip");
    surface_=Surface::ChildLive;
}
bool TimelineView::append(const ReplayCommand& command) {
    if (!editing()) return false;
    if (child_->full()) throw std::length_error("child recording budget reached");
    if (!child_->apply(command)) return false;
    childView_.reset();
    return true;
}
}
