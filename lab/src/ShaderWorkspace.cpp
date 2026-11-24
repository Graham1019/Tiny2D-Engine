#include "ShaderWorkspace.h"
#include "tiny2d_game/SnapshotCodec.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <stdexcept>

namespace lab {
namespace fs=std::filesystem;
namespace {
constexpr std::array<const char*,4> names{"screen.vert","aurora.frag","orbitals.frag","contours.frag"};
const std::vector<std::uint8_t> marker{'T','i','n','y','2','D',' ','G','a','l','l','e','r','y',' ','1','\n'};

bool regularIfPresent(const fs::path& path) {
    const auto status=fs::symlink_status(path);
    if (!fs::exists(status)) return false;
    if (!fs::is_regular_file(status)) throw std::runtime_error("expected a regular workspace file: "+path.string());
    return true;
}

void installIfAbsent(const fs::path& dest,const std::vector<std::uint8_t>& bytes) {
    try { tiny2d_game::writeNewFile(dest,bytes); }
    catch (const fs::filesystem_error& error) {
        if (error.code()!=std::errc::file_exists || !regularIfPresent(dest)) throw;
    }
}
}

bool insideApplication(const fs::path& path) {
    for (const auto& component:fs::weakly_canonical(fs::absolute(path))) {
        auto ext=component.extension().string();
        std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char ch){return char(std::tolower(ch));});
        if (ext==".app") return true;
    }
    return false;
}

fs::path defaultShaderWorkspace() {
    const auto* home=std::getenv("HOME");
    if (!home || !fs::path(home).is_absolute()) throw std::runtime_error("cannot locate user Application Support directory");
    return fs::path(home)/"Library/Application Support/Tiny2D Showcase/Shader Gallery";
}

void requireExternalOutput(const fs::path& path) {
    if (insideApplication(path)) throw std::invalid_argument("choose an output path outside the application: "+path.string());
}

fs::path prepareShaderWorkspace(const fs::path& requested,const fs::path& bundledShaders) {
    if (requested.empty()) throw std::invalid_argument("empty shader workspace path");
    const auto dest=fs::weakly_canonical(fs::absolute(requested));
    requireExternalOutput(dest);
    const auto defaults=fs::canonical(bundledShaders);
    const auto relative=dest.lexically_relative(defaults);
    if (!relative.empty() && *relative.begin()!="..")
        throw std::invalid_argument("choose a workspace outside the original shader directory");
    fs::create_directories(dest);
    const auto ready=dest/".tiny2d-gallery-v1";
    const bool initialized=regularIfPresent(ready);
    if (initialized && tiny2d_game::readBoundedFile(ready,marker.size())!=marker)
        throw std::runtime_error("unknown shader workspace marker: "+ready.string());
    for (const auto* name:names) {
        const auto file=dest/name;
        if (!regularIfPresent(file)) continue;
        // An editable path must not alias the signed originals, even via a hard link.
        for (const auto* original:names)
            if (fs::equivalent(file,defaults/original)) throw std::invalid_argument("workspace file aliases a bundled shader: "+file.string());
    }
    if (!initialized) {
        for (const auto* name:names) {
            const auto file=dest/name;
            if (!regularIfPresent(file)) installIfAbsent(file,tiny2d_game::readBoundedFile(defaults/name,1024*1024));
        }
        installIfAbsent(ready,marker);
    }
    return dest;
}
}
