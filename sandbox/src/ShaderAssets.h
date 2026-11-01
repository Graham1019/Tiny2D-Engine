#pragma once

#include <string>

// Resolve copied assets relative to the actual executable, independent of cwd.
std::string readShaderAsset(const char* filename);

#include <filesystem>
std::filesystem::path assetPath(const char* relative);
