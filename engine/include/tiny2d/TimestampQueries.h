#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <stdexcept>

namespace tiny2d {
// Indexes are offsets into a driver-owned fixed set of GL query names.
class TimestampQueries {
public:
    virtual ~TimestampQueries()=default;
    virtual void stamp(unsigned index)=0;
    virtual bool available(unsigned index)=0;
    virtual std::uint64_t result(unsigned index)=0;
    int bits=0;
    std::string reason;
};
std::unique_ptr<TimestampQueries> openTimestampQueries(unsigned capacity);

enum class QueryStatus { Off, Pending, Complete, Skipped, Invalid };
struct QueryCounts {
    std::uint64_t requested=0, completed=0, pending=0, skipped=0, invalid=0;
};
template<class Frame,unsigned Intervals,unsigned Slots=8>
class TimestampRing {
public:
    static constexpr unsigned capacity=Slots*Intervals*2;
    using Durations=std::array<std::optional<double>,Intervals>;
    explicit TimestampRing(TimestampQueries& queries):query_(queries) {}
    TimestampRing(const TimestampRing&)=delete;
    TimestampRing& operator=(const TimestampRing&)=delete;
    bool hasOutstanding() const { return occupied_!=0; }
    QueryStatus begin(Frame frame) {
        if (!query_.bits) return QueryStatus::Off;
        if (active_) throw std::logic_error("timestamp frame already active");
        ++counts.requested;
        for (unsigned i=0;i<Slots;++i) if (!pool_[i].busy) {
            active_=&pool_[i];
            *active_=Slot{};
            active_->busy=true; active_->base=i*Intervals*2; active_->frame=frame;
            ++occupied_; ++counts.pending;
            return QueryStatus::Pending;
        }
        ++counts.skipped;
        return QueryStatus::Skipped;
    }
    void mark(unsigned interval,bool end) {
        if (!active_) return;
        unsigned offset=interval*2+unsigned(end);
        query_.stamp(active_->base+offset);
        active_->issued.at(offset)=true;
    }
    void finish(Frame frame,bool complete) {
        if (!active_) return;
        active_->frame=frame;
        active_->invalid=!complete || !active_->issued[0] || !active_->issued[1];
        for (unsigned i=0;i<Intervals;++i)
            if (active_->issued[i*2]!=active_->issued[i*2+1]) active_->invalid=true;
        if (active_->invalid) { ++counts.invalid; --counts.pending; }
        active_=nullptr;
    }
    template<class Receive> void poll(Receive receive) {
        for (auto& slot:pool_) {
            if (!slot.busy || &slot==active_) continue;
            bool ready=true;
            for (unsigned i=0;i<slot.issued.size();++i)
                if (slot.issued[i] && !query_.available(slot.base+i)) { ready=false; break; }
            if (!ready) continue;
            Durations times{};
            bool valid=!slot.invalid;
            std::uint64_t totalStart=0,totalEnd=0;
            for (unsigned i=0;valid && i<Intervals;++i) {
                if (slot.issued[2*i]!=slot.issued[2*i+1]) { valid=false; break; }
                if (!slot.issued[2*i]) continue;
                auto start=query_.result(slot.base+2*i), end=query_.result(slot.base+2*i+1);
                auto mask=UINT64_MAX >> (64-query_.bits);
                start &= mask; end &= mask;
                // A backwards timestamp can be wrap or a discontinuity. Exclude the frame.
                if (!i) { totalStart=start; totalEnd=end; }
                if (end<start || end-start>mask/2 || start<totalStart || end>totalEnd) valid=false;
                else times[i]=double(end-start)/1000.;
            }
            if (!slot.invalid) {
                --counts.pending;
                if (valid) ++counts.completed; else ++counts.invalid;
            }
            slot.busy=false; --occupied_;
            receive(slot.frame,valid?QueryStatus::Complete:QueryStatus::Invalid,times);
        }
    }
    QueryCounts counts;
private:
    struct Slot {
        Frame frame{};
        unsigned base=0;
        bool busy=false,invalid=false;
        std::array<bool,2*Intervals> issued{};
    };
    TimestampQueries& query_;
    std::array<Slot,Slots> pool_{};
    Slot* active_=nullptr;
    unsigned occupied_=0;
};
}
