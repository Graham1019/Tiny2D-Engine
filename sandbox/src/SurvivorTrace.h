#pragma once

#include "tiny2d_game/GameSession.h"
#include <cstdio>
#include <filesystem>
#include <string>

// Optional observations for finite verification, outside the simulation clock.
class SurvivorCsv {
public:
    explicit SurvivorCsv(const std::filesystem::path& path);
    ~SurvivorCsv();
    SurvivorCsv(const SurvivorCsv&) = delete;
    SurvivorCsv& operator=(const SurvivorCsv&) = delete;
    void line(const std::string& row);
    void finish();
private:
    std::FILE* stream_;
};

const char* survivorTraceHeader();
std::string survivorTraceRow(const tiny2d_game::GameSession&,std::uint64_t previousEvent);
