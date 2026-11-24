#include "ShaderAssets.h"

#include <mach-o/dyld.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

std::filesystem::path tiny2dExecutablePath() {
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> executable(size);
    if (_NSGetExecutablePath(executable.data(), &size) != 0) {
        throw std::runtime_error("Cannot resolve executable path for shader assets");
    }
    return std::filesystem::canonical(executable.data());
}

std::filesystem::path assetPath(const char* relative) {
    const auto directory = tiny2dExecutablePath().parent_path();
    const bool bundle = directory.filename() == "MacOS" &&
                        directory.parent_path().filename() == "Contents";
    const auto root = bundle ? directory.parent_path() / "Resources/assets" : directory / "assets";
    const auto result = root / relative;
    if (!std::filesystem::is_regular_file(result))
        throw std::runtime_error("Missing Tiny2D asset: " + result.string());
    return result;
}

std::string readShaderAsset(const char* filename) {
    return readShaderFile(assetPath((std::string("shaders/")+filename).c_str()));
}

std::string readShaderFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open shader asset: " + path.string());
    std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (file.bad()) throw std::runtime_error("Cannot read shader asset: " + path.string());
    return source;
}
