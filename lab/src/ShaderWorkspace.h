#pragma once
#include <filesystem>

namespace lab {
std::filesystem::path defaultShaderWorkspace();
// Copies missing defaults on first use. A completed workspace belongs to the
// user, including edits or later deletions. Concurrent initialization is safe.
std::filesystem::path prepareShaderWorkspace(const std::filesystem::path& requested,
                                             const std::filesystem::path& bundledShaders);
}
