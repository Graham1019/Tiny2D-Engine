#pragma once
#include "tiny2d/SpriteRenderer.h"
#include "tiny2d/RenderStats.h"
#include <memory>
namespace tiny2d {
// begin -> draw/flush -> end. Nested begin and inactive draw/flush/end throw.
// Empty flush is a no-op. Invalid draw leaves the queue unchanged.
// Textures must remain alive until flush. No external GL state changes between
// begin/end: flush then end before switching renderers or framebuffer/state.
// Restores program, VAO, array buffer, textures, active unit and blend at end.
// Depth/scissor/cull and framebuffer are caller-owned and never changed.
class BatchRenderer {
public:
    explicit BatchRenderer(std::size_t capacity=1000, unsigned textureSlots=8);
    ~BatchRenderer();
    BatchRenderer(BatchRenderer&&) noexcept;
    // Construction transfers even an active pass without touching GL state.
    // Assignment requires both objects inactive and their owning context current.
    // Active source/target throws without mutation; self-assignment is a no-op.
    BatchRenderer& operator=(BatchRenderer&&);
    BatchRenderer(const BatchRenderer&)=delete;
    BatchRenderer& operator=(const BatchRenderer&)=delete;
    void beginFrame(const Camera2D&);
    void draw(const Texture2D&,Vector2,Vector2,float radians=0,Color tint={});
    void flush();
    void endFrame();
    const RenderStats& stats() const;
    unsigned textureSlots() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Impl& get() const;
};
}
