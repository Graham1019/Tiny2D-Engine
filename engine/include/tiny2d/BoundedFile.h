#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace tiny2d {

// Inspect the opened descriptor and reject non-regular files, excess bytes and short reads.
std::vector<std::uint8_t> readBoundedFile(const std::filesystem::path& path, std::size_t limit);

} // namespace tiny2d
