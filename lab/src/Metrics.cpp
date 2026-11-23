#include "Metrics.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>

namespace lab {
using tiny2d::QueryStatus;
namespace {
const char* status(QueryStatus s) {
    switch(s) {
    case QueryStatus::Off: return "off_or_unavailable";
    case QueryStatus::Pending: return "pending";
    case QueryStatus::Skipped: return "skipped";
    case QueryStatus::Invalid: return "invalid";
    case QueryStatus::Complete: return "completed";
    }
    throw std::logic_error("unknown query status");
}
}
Metrics::Metrics(const std::string& path) {
    if (path.empty()) return;
    retired_.reserve(9);
    out_.exceptions(std::ios::badbit|std::ios::failbit);
    try { out_.open(path); }
    catch (const std::ios_base::failure& e) { throw std::runtime_error("open metrics "+path+": "+e.what()); }
    out_<<std::setprecision(12)<<"record,frame,tick,epoch,generation,measurement,width,height,logical_w,logical_h,view,bloom,strength_tenths,hud,inspector,timing,preview_mask,pass_mask,timestamp_bits,query_status,received_frame,total_us";
    for (auto label:passNames) out_<<','<<label<<"_us";
    out_<<",update_us,prepare_us,gui_build_us,query_poll_us,passes,draws,world_sprites,batch_vertex_bytes,gui_vertices,gui_indices,gui_vertex_bytes,gui_index_bytes,font_upload_bytes,hdr_bytes,preview_bytes,font_bytes";
    for (auto label:passNames) out_<<','<<label<<"_draws";
    for (auto label:passNames) out_<<','<<label<<"_batch_bytes";
    out_<<'\n';
}
void Metrics::prefix(const FrameInfo& f,const char* kind,QueryStatus query) {
    out_<<kind<<','<<f.id<<','<<f.tick<<','<<f.epoch<<','<<f.generation<<','<<f.measurement<<','
        <<f.pixels.width<<','<<f.pixels.height<<','<<f.logical.width<<','<<f.logical.height<<','
        <<name(f.display.view)<<','<<f.display.bloom<<','<<f.display.strength<<','<<f.display.hud<<','
        <<f.inspector<<','<<f.timing<<','<<f.previews<<','<<f.mask<<',';
    if (f.timestampBits>=0) out_<<f.timestampBits;
    out_<<','<<(query==QueryStatus::Off ? (f.timing?"unavailable":"off") : status(query))<<',';
}
void Metrics::record(const CpuSample& c) {
    if (!out_.is_open()) return;
    prefix(c.info,"cpu",c.status);
    out_<<','<<c.total;
    const auto& s=c.stats;
    for (unsigned i=0;i<PassCount;++i) { out_<<','; if (s.mask&(1u<<i)) out_<<s.cpuUs[i]; }
    out_<<','<<c.update<<','<<c.prepare<<',';
    if (c.info.inspector || c.build>0) out_<<c.build;
    out_<<','; if (c.poll>0) out_<<c.poll;
    out_<<','<<s.passes<<','<<s.draws<<','<<s.worldSprites<<','<<s.bytes
        <<','<<s.guiVertices<<','<<s.guiIndices<<','<<s.vertexBytes<<','<<s.indexBytes<<','<<s.fontUpload
        <<','<<c.hdrBytes<<','<<c.previewBytes<<','<<s.fontBytes;
    for (auto draws:s.passDraws) out_<<','<<draws;
    for (auto bytes:s.passBytes) out_<<','<<bytes;
    out_<<'\n';
}
void Metrics::write(const GpuSample& g) {
    if (!out_.is_open()) return;
    prefix(g.info,"gpu",g.status); out_<<g.received;
    for (auto us:g.us) {out_<<','; if (g.status==QueryStatus::Complete && us) out_<<*us;}
    out_<<std::string(16+2*PassCount,',')<<'\n';
}
void Metrics::retired(GpuSample g) { if (out_.is_open()) retired_.push_back(std::move(g)); }
void Metrics::flushRetired() {
    for (const auto& result:retired_) write(result);
    retired_.clear();
}
void Metrics::close() {
    if (!out_.is_open()) return;
    flushRetired();
    out_.close();
}
Metrics::~Metrics() {
    try {close();} catch (const std::exception& e) {std::cerr<<"close metrics: "<<e.what()<<'\n';}
}
void report(const std::vector<CpuSample>& samples) {
    if (samples.empty()) return;
    auto metric=[&](const char* label,auto value) {
        std::vector<double> sorted;
        for (const auto& s:samples) if (auto v=value(s)) sorted.push_back(*v);
        if (sorted.empty()) return;
        std::sort(sorted.begin(),sorted.end());
        auto q=[&](double p){ return sorted[std::size_t(std::ceil(p*sorted.size()))-1]; };
        std::cout<<"metric="<<label<<" n="<<sorted.size()<<" p50="<<q(.5)<<" p95="<<q(.95)<<" p99="<<q(.99)<<'\n';
    };
    std::cout<<"warmup=120 samples="<<samples.size()<<" clock=steady units=us\n";
    metric("frame",[](const auto& c){ return std::optional<double>(c.total); });
    metric("fixed_update",[](const auto& c){return std::optional<double>(c.update);});
    metric("prepare",[](const auto& c){return std::optional<double>(c.prepare);});
    metric("draw_calls",[](const auto& c){return std::optional<double>(c.stats.draws);});
    metric("sprites",[](const auto& c){return std::optional<double>(c.stats.sprites);});
    metric("uploaded_bytes",[](const auto& c){return std::optional<double>(c.stats.bytes+c.stats.vertexBytes+c.stats.indexBytes+c.stats.fontUpload);});
    for (unsigned i=0;i<PassCount;++i) metric(passNames[i],[&](const auto& c)->std::optional<double>{
        if (c.stats.mask&(1u<<i)) return c.stats.cpuUs[i]; return {};
    });
}
}
