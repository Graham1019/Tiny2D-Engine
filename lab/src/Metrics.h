#pragma once
#include "Observation.h"
#include <fstream>

namespace lab {
class Metrics {
public:
    explicit Metrics(const std::string& path);
    ~Metrics();
    void record(const CpuSample&);
    void retired(GpuSample);
    void flushRetired();
    void close();
private:
    std::ofstream out_;
    std::vector<GpuSample> retired_; // At most one ring of arrivals and one partial frame.
    void write(const GpuSample&);
    void prefix(const FrameInfo&,const char* kind,tiny2d::QueryStatus);
};
void report(const std::vector<CpuSample>& measurement);
}
