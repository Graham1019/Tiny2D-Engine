#include "ShaderAssets.h"

#include <mach-o/dyld.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

std::filesystem::path assetPath(const char* relative) {
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> executable(size);
    if (_NSGetExecutablePath(executable.data(), &size) != 0) {
        throw std::runtime_error("Cannot resolve executable path for shader assets");
    }
    return std::filesystem::canonical(executable.data()).parent_path() /
                      "assets" / relative;
}

std::string readShaderAsset(const char* filename) {
    const auto path = assetPath((std::string("shaders/")+filename).c_str());
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open shader asset: " + path.string());
    std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (file.bad()) throw std::runtime_error("Cannot read shader asset: " + path.string());
    return source;
}
