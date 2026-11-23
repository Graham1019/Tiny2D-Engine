#pragma once
#include <array>
#include <stdexcept>

namespace lab {
enum class GalleryEffect { Aurora, Orbitals, Contours };
inline constexpr std::array galleryEffects{GalleryEffect::Aurora, GalleryEffect::Orbitals, GalleryEffect::Contours};
inline constexpr const char* galleryName(GalleryEffect effect) {
    switch (effect) {
    case GalleryEffect::Aurora: return "aurora";
    case GalleryEffect::Orbitals: return "orbitals";
    case GalleryEffect::Contours: return "contours";
    }
    throw std::invalid_argument("unknown gallery effect");
}
}
