#include "ShaderGallery.h"
#include "ShaderAssets.h"

namespace lab {
namespace {
ShaderReloadManager load(GalleryEffect effect,const std::filesystem::path& workspace) {
    const auto fragment = std::string("shaders/gallery/") + galleryName(effect) + ".frag";
    const auto vertexFile=assetPath("shaders/gallery/screen.vert"),fragmentFile=assetPath(fragment.c_str());
    if (workspace.empty()) return {vertexFile,fragmentFile};
    return {workspace/vertexFile.filename(),workspace/fragmentFile.filename(),
            ShaderReloadManager::Sources{vertexFile,fragmentFile}};
}
}
ShaderGallery::ShaderGallery(const std::filesystem::path& workspace)
    : shaders_{load(GalleryEffect::Aurora,workspace),load(GalleryEffect::Orbitals,workspace),
               load(GalleryEffect::Contours,workspace)} {}
}
