#pragma once

#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>

namespace tiny2d {

bool insideApplication(const std::filesystem::path& path);
void requireExternalOutput(const std::filesystem::path& path);
void resolveNewOutputFiles(std::initializer_list<std::string*> files);

// A packaged CLI run writes only to private staging paths. Publish completed
// files on successful exit, without ever opening the caller's final names.
class PackagedOutputs {
public:
    PackagedOutputs() = default;
    PackagedOutputs(const PackagedOutputs&) = delete;
    PackagedOutputs& operator=(const PackagedOutputs&) = delete;
    ~PackagedOutputs();

    void prepare(std::initializer_list<std::string*> files, std::string* directory = nullptr);
    // A workspace requested within a new tour travels with that directory.
    std::filesystem::path stagedDirectoryPath(const std::filesystem::path& path) const;
    void publish();

private:
    struct Entry {
        std::filesystem::path target, scratch, staged;
        bool directory = false;
        bool published = false;
    };
    std::vector<Entry> entries_;
};

} // namespace tiny2d
