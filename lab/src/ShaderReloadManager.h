#pragma once

#include "tiny2d/ShaderProgram.h"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace lab {

struct ShaderReloadStatus {
    std::uint64_t version = 0; // Last successful generation, starting at 1.
    bool success = false;      // Result of the latest load attempt.
    std::string error;
};

// Synchronous, frame-boundary owner for one gallery exhibit. Construct, access,
// reload and destroy on the main thread, with the same live GL context current.
class ShaderReloadManager {
public:
    using Sources=std::pair<std::filesystem::path,std::filesystem::path>;
    ShaderReloadManager(std::filesystem::path vertexPath,std::filesystem::path fragmentPath,
                        std::optional<Sources> startupFallback={});
    ShaderReloadManager(const ShaderReloadManager&) = delete;
    ShaderReloadManager& operator=(const ShaderReloadManager&) = delete;

    // File, driver and interface errors leave the previous version drawable.
    // Calling from a different context or thread is a logic error.
    bool reload();
    const tiny2d::ShaderProgram& program() const { return *active_; }
    const ShaderReloadStatus& status() const { return status_; }
    const std::filesystem::path& vertexPath() const { return vertexPath_; }
    const std::filesystem::path& fragmentPath() const { return fragmentPath_; }

private:
    std::filesystem::path vertexPath_, fragmentPath_;
    GLFWwindow* context_;
    std::optional<tiny2d::ShaderProgram> active_;
    ShaderReloadStatus status_;
};

}
