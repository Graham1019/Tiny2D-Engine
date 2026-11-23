#pragma once
#include "GalleryEffect.h"
#include "ShaderReloadManager.h"

namespace lab {
class ShaderGallery {
public:
    // Compile the entire exhibit once, in the Lab's current GL context.
    ShaderGallery();
    ShaderReloadManager& shader(GalleryEffect effect) { return shaders_.at(static_cast<std::size_t>(effect)); }
private:
    std::array<ShaderReloadManager, galleryEffects.size()> shaders_;
};
}
