#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

// Resolve copied assets relative to the actual executable, independent of cwd.
std::string readShaderAsset(const char* filename);

std::filesystem::path tiny2dExecutablePath();
std::filesystem::path assetPath(const char* relative);
inline constexpr std::size_t maxShaderBytes = 1024 * 1024;
std::string readShaderFile(const std::filesystem::path& path);
