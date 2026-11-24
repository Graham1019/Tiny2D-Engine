#pragma once
#include <filesystem>

namespace lab {
bool insideApplication(const std::filesystem::path&);
std::filesystem::path defaultShaderWorkspace();
void requireExternalOutput(const std::filesystem::path&);
// Copies missing defaults on first use. A completed workspace belongs to the
// user, including edits or later deletions. Concurrent initialization is safe.
std::filesystem::path prepareShaderWorkspace(const std::filesystem::path& requested,
                                             const std::filesystem::path& bundledShaders);
}
