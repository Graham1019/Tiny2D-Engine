#pragma once

#include <string>

struct SurvivorOptions {
    unsigned frames=0, difficulty=3;
    bool enhanced=false, bloom=true;
    std::string capture,trace,metrics;
};

void runSurvivor(const SurvivorOptions&);
void runSurvivor(unsigned frames,unsigned level=3);
