#pragma once

#include "tiny2d/ImageLoader.h"

namespace lab {
enum class NormalMaterialKind { Flat, ProceduralTile };
struct WorldNormal { float x, y, z; };

inline constexpr int normalTileSize = 64;
inline constexpr float normalTilePeriod = 32.f; // World units along both X and Y.

struct NormalMaterial2D {
    NormalMaterialKind kind = NormalMaterialKind::ProceduralTile;
    float strength = 1.f;
    static constexpr float maxStrength = 2.f;
    void validate() const;
    // Sample the periodic height field at a texel center. +Z points to the viewer.
    WorldNormal normalAtTexel(int x, int y) const;
};

// Linear vector data, RGBA8. R/G encode 128 + 127 * X/Y, B encodes 255 * Z.
// Columns increase along world +X, rows along world +Y. Alpha is 255.
tiny2d::ImageData makeNormalTile();
}
