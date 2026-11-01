#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>
namespace tiny2d {
// Straight RGBA8, top row first. Sprite UV (0,0) addresses that top row.
struct ImageData { int width=0,height=0; std::vector<std::uint8_t> pixels; };
class ImageLoader { public: static ImageData loadPNG(const std::filesystem::path& path); };
}
