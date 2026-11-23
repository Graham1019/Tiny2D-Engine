#include "NormalMaterial.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lab {
namespace {
constexpr float step = normalTilePeriod / normalTileSize;

float texelCenter(int index) {
    const int wrapped = (index % normalTileSize + normalTileSize) % normalTileSize;
    return (wrapped + .5f) * step;
}
float smoothStep(float low, float high, float value) {
    const float t = std::clamp((value - low) / (high - low), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}
float height(float x, float y) {
    const float edgeX = std::min(x, normalTilePeriod - x);
    const float edgeY = std::min(y, normalTilePeriod - y);
    const float panel = smoothStep(.75f, 5.f, edgeX) * smoothStep(.75f, 5.f, edgeY);
    // Shallow off-center registration groove machined into the plateau.
    const float grooveX = smoothStep(6.f, 8.f, x) * (1.f - smoothStep(17.f, 19.f, x));
    const float grooveY = smoothStep(21.f, 23.f, y) * (1.f - smoothStep(24.f, 26.f, y));
    return 3.5f * panel - 1.25f * grooveX * grooveY;
}
}

void NormalMaterial2D::validate() const {
    if (kind != NormalMaterialKind::Flat && kind != NormalMaterialKind::ProceduralTile)
        throw std::invalid_argument("invalid normal material kind");
    if (!(strength >= 0.f && strength <= maxStrength))
        throw std::invalid_argument("normal strength must be finite and in [0,2]");
}
WorldNormal NormalMaterial2D::normalAtTexel(int x, int y) const {
    validate();
    if (kind == NormalMaterialKind::Flat || strength == 0.f) return {0.f, 0.f, 1.f};
    // Reduce the indices before adding neighbors so any signed int is safe.
    x %= normalTileSize;
    y %= normalTileSize;
    const float wx = texelCenter(x), wy = texelCenter(y);
    const float nx = strength * (height(texelCenter(x-1), wy) - height(texelCenter(x+1), wy)) / (2.f * step);
    const float ny = strength * (height(wx, texelCenter(y-1)) - height(wx, texelCenter(y+1))) / (2.f * step);
    const float length = std::sqrt(nx*nx + ny*ny + 1.f);
    return {nx / length, ny / length, 1.f / length};
}
tiny2d::ImageData makeNormalTile() {
    tiny2d::ImageData tile{normalTileSize, normalTileSize, {}};
    tile.pixels.reserve(normalTileSize * normalTileSize * 4);
    const NormalMaterial2D original{};
    for (int y=0; y<normalTileSize; ++y) {
        for (int x=0; x<normalTileSize; ++x) {
            const auto n = original.normalAtTexel(x, y);
            tile.pixels.push_back(static_cast<std::uint8_t>(std::lround(128.f + 127.f*n.x)));
            tile.pixels.push_back(static_cast<std::uint8_t>(std::lround(128.f + 127.f*n.y)));
            tile.pixels.push_back(static_cast<std::uint8_t>(std::lround(255.f*n.z)));
            tile.pixels.push_back(255);
        }
    }
    return tile;
}
}
