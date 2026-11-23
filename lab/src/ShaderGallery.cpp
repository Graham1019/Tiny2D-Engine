#include "ShaderGallery.h"
#include "ShaderAssets.h"

namespace lab {
namespace {
ShaderReloadManager load(GalleryEffect effect) {
    const auto fragment = std::string("shaders/gallery/") + galleryName(effect) + ".frag";
    return {assetPath("shaders/gallery/screen.vert"), assetPath(fragment.c_str())};
}
}
ShaderGallery::ShaderGallery()
    : shaders_{load(GalleryEffect::Aurora), load(GalleryEffect::Orbitals), load(GalleryEffect::Contours)} {}
}
