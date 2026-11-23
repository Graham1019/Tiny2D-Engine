#pragma once

#include <filesystem>
#include <string>

// Resolve copied assets relative to the actual executable, independent of cwd.
std::string readShaderAsset(const char* filename);

std::filesystem::path assetPath(const char* relative);
std::string readShaderFile(const std::filesystem::path& path);
