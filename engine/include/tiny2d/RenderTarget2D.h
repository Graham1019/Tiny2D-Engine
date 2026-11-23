#pragma once
#include "tiny2d/detail/GLObject.h"
#include <cstdint>
#include <string>

namespace tiny2d {
// Immutable single-sample RGBA attachment. Binding is a pass-local
// borrow. Unbind before moving/destroying a target or replacing its group.
class RenderTarget2D {
public:
    static constexpr std::uint64_t budget = 256ull * 1024 * 1024;
    static std::uint64_t storageBytes(int width, int height);
    enum class Format { HDR16F, Display8 };
    RenderTarget2D(const std::string& name,int width,int height,Format format=Format::HDR16F);
    unsigned colorName() const { texture_.requireCurrent("borrow color texture"); return texture_.id(); }
    RenderTarget2D(RenderTarget2D&&) noexcept;
    RenderTarget2D& operator=(RenderTarget2D&&) = delete;
    int width() const { return width_; }
    int height() const { return height_; }
    void bindDraw() const;
    void bindColor(unsigned unit) const;
private:
    int width_, height_;
    detail::GLObject texture_{detail::ObjectKind::Texture};
    detail::GLObject fbo_{detail::ObjectKind::Framebuffer};
};
}
