#include "tiny2d/RenderTarget2D.h"
#include "OpenGL.h"
#include <stdexcept>
#include <utility>

namespace tiny2d {
std::uint64_t RenderTarget2D::storageBytes(int width, int height) {
    if (width < 1 || height < 1 || std::uint64_t(width) > budget / 8 / height)
        throw std::invalid_argument("invalid RGBA16F dimensions or attachment exceeds 256 MiB");
    return std::uint64_t(width) * height * 8;
}

RenderTarget2D::RenderTarget2D(const std::string& name, int w, int h) : width_(w), height_(h) {
    const auto label = name + " " + std::to_string(w) + "x" + std::to_string(h) + " RGBA16F";
    storageBytes(w,h);
    GLint limit;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &limit);
    if (w > limit || h > limit) throw std::invalid_argument(label + " exceeds GL_MAX_TEXTURE_SIZE");

    // A null data pointer means no upload only with the unpack buffer unbound.
    // No pixel data is supplied, so alignment, strides and skip counts are immaterial
    // and can remain exactly as the caller set them.
    struct Restore {
        GLint texture, draw, unpack;
        ~Restore() {
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack);
            glBindTexture(GL_TEXTURE_2D, texture);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
        }
    } old{};
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old.texture);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old.draw);
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &old.unpack);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, texture_.id());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_.id());
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_.id(), 0);
    const GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    const auto diagnostic = label + " framebuffer status=" + std::to_string(status);
    detail::checkGL(diagnostic.c_str());
    if (status != GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error(diagnostic);
}

RenderTarget2D::RenderTarget2D(RenderTarget2D&& other) noexcept
    : width_(std::exchange(other.width_,0)), height_(std::exchange(other.height_,0)),
      texture_(std::move(other.texture_)), fbo_(std::move(other.fbo_)) {}

void RenderTarget2D::bindDraw() const {
    fbo_.requireCurrent("RenderTarget2D::bindDraw");
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_.id());
    glViewport(0,0,width_,height_);
}
void RenderTarget2D::bindColor(unsigned unit) const {
    texture_.requireCurrent("RenderTarget2D::bindColor");
    if (unit > 1) throw std::invalid_argument("render target sampling supports units 0 and 1");
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture_.id());
}
}
