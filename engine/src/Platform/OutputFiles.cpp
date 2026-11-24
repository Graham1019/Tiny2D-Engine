#include "tiny2d/OutputFiles.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <unistd.h>

namespace tiny2d {
namespace fs = std::filesystem;

bool insideApplication(const fs::path& path) {
    for (const auto& component : fs::weakly_canonical(fs::absolute(path))) {
        auto ext = component.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char ch) { return char(std::tolower(ch)); });
        if (ext == ".app") return true;
    }
    return false;
}

void requireExternalOutput(const fs::path& path) {
    if (insideApplication(path))
        throw std::invalid_argument("choose an output path outside the application: " + path.string());
}

PackagedOutputs::~PackagedOutputs() {
    for (const auto& entry : entries_) {
        if (entry.scratch.empty()) continue;
        std::error_code ignored;
        fs::remove_all(entry.scratch, ignored);
    }
}

namespace {
std::vector<std::string*> resolveNewNames(std::initializer_list<std::string*> files,
                                          std::string* directory, bool externalOnly) {
    std::vector<std::string*> requested;
    auto resolve = [&](std::string* name) {
        if (!name || name->empty()) return;
        const auto target = fs::weakly_canonical(fs::absolute(*name));
        if (externalOnly) requireExternalOutput(target);
        // symlink_status on the original name also catches dangling links.
        if (fs::exists(fs::symlink_status(*name)))
            throw std::invalid_argument("output already exists: " + *name);
        for (const auto* prior : requested)
            if (*prior == target)
                throw std::invalid_argument("outputs require distinct new paths: " + *name);
        *name = target.string();
        requested.push_back(name);
    };
    for (auto* file : files) resolve(file);
    resolve(directory);
    return requested;
}
}

void resolveNewOutputFiles(std::initializer_list<std::string*> files) {
    resolveNewNames(files, nullptr, false);
}

void PackagedOutputs::prepare(std::initializer_list<std::string*> files, std::string* directory) {
    const auto requested = resolveNewNames(files, directory, true);
    // mkdtemp gives the historical PNG and CSV writers their own mode-0700
    // directory on the output filesystem. Neither writer opens the final path.
    for (auto* name : requested) {
        entries_.push_back({*name, {}, {}, name == directory});
        auto& entry = entries_.back();
        auto parent = entry.target.parent_path();
        while (!fs::exists(parent)) parent = parent.parent_path();
        auto pattern = (parent / ".tiny2d-output-XXXXXX").string();
        if (!::mkdtemp(pattern.data()))
            throw fs::filesystem_error("stage new output", entry.target,
                                       std::error_code(errno, std::generic_category()));
        entry.scratch = pattern;
        entry.staged = entry.scratch / "output";
        *name = entry.staged.string();
    }

    // A caller can request a launcher capture or CSV inside the new tour
    // directory. Link it into the completed tour before publishing the directory.
    for (auto& file : entries_)
        if (!file.directory) file.target = stagedDirectoryPath(file.target);
}

fs::path PackagedOutputs::stagedDirectoryPath(const fs::path& path) const {
    if (entries_.empty() || !entries_.back().directory) return path;
    const auto& tour = entries_.back();
    const auto relative = fs::weakly_canonical(fs::absolute(path)).lexically_relative(tour.target);
    if (!relative.empty() && *relative.begin() != "..")
        return relative == "." ? tour.staged : tour.staged / relative;
    return path;
}

void PackagedOutputs::publish() {
    try {
        for (auto& entry : entries_) {
            requireExternalOutput(entry.target);
            if (entry.directory) {
                // macOS supports exclusive rename of a completed directory.
                if (::renamex_np(entry.staged.c_str(), entry.target.c_str(), RENAME_EXCL) != 0)
                    throw fs::filesystem_error("publish new output directory", entry.target,
                                               std::error_code(errno, std::generic_category()));
            } else {
                if (!fs::is_regular_file(fs::symlink_status(entry.staged)))
                    throw std::runtime_error("missing completed output: " + entry.target.string());
                fs::create_hard_link(entry.staged, entry.target);
            }
            entry.published = true;
        }
    } catch (...) {
        // Publication of a later output can lose a race with another process.
        // Only remove links to this run's own completed files. The directory
        // is published last, so a failed transaction never exposes a partial tour.
        for (const auto& entry : entries_) {
            if (!entry.published) continue;
            std::error_code ignored;
            if (fs::equivalent(entry.staged, entry.target, ignored)) fs::remove(entry.target, ignored);
        }
        throw;
    }
}

} // namespace tiny2d
