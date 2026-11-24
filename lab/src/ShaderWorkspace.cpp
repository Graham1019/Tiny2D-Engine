#include "ShaderWorkspace.h"
#include "ShaderAssets.h"
#include "tiny2d_game/SnapshotCodec.h"
#include "tiny2d/OutputFiles.h"
#include <array>
#include <cstdlib>
#include <stdexcept>

namespace lab {
namespace fs=std::filesystem;
namespace {
constexpr std::array<const char*,4> names{"screen.vert","aurora.frag","orbitals.frag","contours.frag"};
const std::vector<std::uint8_t> marker{'T','i','n','y','2','D',' ','G','a','l','l','e','r','y',' ','1','\n'};

bool workspaceEntryExists(const fs::path& path) {
    const auto status=fs::symlink_status(path);
    if (fs::is_symlink(status)) throw std::runtime_error("workspace file is a symbolic link: "+path.string());
    return fs::exists(status);
}

void installIfAbsent(const fs::path& dest,const std::vector<std::uint8_t>& bytes) {
    try { tiny2d_game::writeNewFile(dest,bytes); }
    catch (const fs::filesystem_error& error) {
        if (error.code()!=std::errc::file_exists || !fs::is_regular_file(fs::symlink_status(dest))) throw;
    }
}
}

fs::path defaultShaderWorkspace() {
    const auto* home=std::getenv("HOME");
    if (!home || !fs::path(home).is_absolute()) throw std::runtime_error("cannot locate user Application Support directory");
    return fs::path(home)/"Library/Application Support/Tiny2D Showcase/Shader Gallery";
}

fs::path prepareShaderWorkspace(const fs::path& requested,const fs::path& bundledShaders) {
    if (requested.empty()) throw std::invalid_argument("empty shader workspace path");
    const auto dest=fs::weakly_canonical(fs::absolute(requested));
    tiny2d::requireExternalOutput(dest);
    const auto defaults=fs::canonical(bundledShaders);
    const auto relative=dest.lexically_relative(defaults);
    if (!relative.empty() && *relative.begin()!="..")
        throw std::invalid_argument("choose a workspace outside the original shader directory");
    fs::create_directories(dest);
    const auto ready=dest/".tiny2d-gallery-v1";
    const bool initialized=workspaceEntryExists(ready);
    if (initialized && tiny2d::readBoundedFile(ready,marker.size())!=marker)
        throw std::runtime_error("unknown shader workspace marker: "+ready.string());
    for (const auto* name:names) {
        const auto file=dest/name;
        if (!workspaceEntryExists(file)) continue;
        // An editable path must not alias the signed originals, even via a hard link.
        for (const auto* original:names)
            if (fs::equivalent(file,defaults/original)) throw std::invalid_argument("workspace file aliases a bundled shader: "+file.string());
    }
    if (!initialized) {
        for (const auto* name:names) {
            const auto file=dest/name;
            // Leave existing edits in place. The loader can fall back for an unreadable edit.
            if (!workspaceEntryExists(file)) installIfAbsent(file,tiny2d::readBoundedFile(defaults/name,maxShaderBytes));
        }
        installIfAbsent(ready,marker);
    }
    return dest;
}
}
