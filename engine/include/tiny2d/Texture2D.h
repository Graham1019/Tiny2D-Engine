#pragma once
#include "tiny2d/ImageLoader.h"
#include "tiny2d/detail/GLObject.h"
namespace tiny2d {
enum class TextureFilter { Linear, Nearest };
// Owning context must be current on the main thread through destruction.
class Texture2D {
public:
    explicit Texture2D(const ImageData& image,TextureFilter filter=TextureFilter::Linear);
    Texture2D(const Texture2D&)=delete;
    Texture2D& operator=(const Texture2D&)=delete;
    Texture2D(Texture2D&&) noexcept=default;
    Texture2D& operator=(Texture2D&&) noexcept=default;
    void bind(unsigned unit=0) const;
    int width() const { return width_; }
    int height() const { return height_; }
    unsigned nativeHandle() const { return object_.id(); }
private:
    detail::GLObject object_;
    int width_,height_;
    unsigned maxUnits_=0;
};
}
