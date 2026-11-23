#pragma once
#include "Observation.h"
#include <fstream>

namespace lab {
class Metrics {
public:
    explicit Metrics(const std::string& path);
    void record(const CpuSample&);
    void record(const GpuSample&);
    void close();
private:
    std::ofstream out_;
    void prefix(const FrameInfo&,const char* kind,tiny2d::QueryStatus);
};
void report(const std::vector<CpuSample>& measurement);
}
