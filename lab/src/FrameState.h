#pragma once
#include "OpenGL.h"
#include <array>
#include <vector>

namespace lab {
// Synchronous, main-thread scope. No callbacks or context switches inside it.
// BatchRenderer independently restores its blend functions, VAO and buffer state.
class FrameState {
    static constexpr GLenum flags[] = {GL_SCISSOR_TEST, GL_DEPTH_TEST, GL_STENCIL_TEST,
        GL_CULL_FACE, GL_FRAMEBUFFER_SRGB, GL_RASTERIZER_DISCARD, GL_COLOR_LOGIC_OP,
        GL_SAMPLE_ALPHA_TO_COVERAGE, GL_SAMPLE_COVERAGE, GL_SAMPLE_MASK, GL_PRIMITIVE_RESTART, GL_DITHER};
    std::array<GLboolean,sizeof(flags)/sizeof(*flags)> enabled_{};
    GLint draw_, read_, viewport_[4], program_, vao_, arrayBuffer_, active_, textures_[3], samplers_[3];
    GLint polygon_[2], backBuffer_;
    GLboolean depth_;
    struct BufferState {
        GLboolean blend, color[4];
        GLint eqRGB,eqAlpha,srcRGB,dstRGB,srcAlpha,dstAlpha;
    };
    bool indexedBlend_;
    std::vector<BufferState> buffers_;
public:
    FrameState();
    ~FrameState() noexcept;
    FrameState(const FrameState&) = delete;
    FrameState& operator=(const FrameState&) = delete;
};
}
