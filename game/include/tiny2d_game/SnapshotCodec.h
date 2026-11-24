#pragma once

#include "tiny2d/BoundedFile.h"
#include "tiny2d_game/NavigationSnapshot.h"
#include <filesystem>
#include <string>

namespace tiny2d_game {

inline constexpr std::uint32_t snapshotFormatVersion = 1;
inline constexpr std::size_t maxSnapshotBytes = 1024 * 1024;

// Decode checks the wire representation. NavigationSession::fromSnapshot
// validates simulation semantics and rebuilds derived data before publication.
std::vector<std::uint8_t> encodeSnapshot(const NavigationSnapshot&);
NavigationSnapshot decodeSnapshot(const std::vector<std::uint8_t>&);
std::uint64_t stateHash(const NavigationSnapshot&);
std::string firstDifference(const NavigationSnapshot&, const NavigationSnapshot&);

using tiny2d::readBoundedFile;
// Disk writes publish a verified sibling temporary file only to a new name.
void writeNewFile(const std::filesystem::path&, const std::vector<std::uint8_t>&);

} // namespace tiny2d_game
