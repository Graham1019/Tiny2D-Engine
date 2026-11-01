#pragma once
#include "tiny2d/Camera2D.h"
#include "tiny2d/RenderStats.h"
#include "tiny2d/Texture2D.h"
#include "tiny2d/ShaderProgram.h"
#include "tiny2d/VertexBuffer.h"
#include "tiny2d/VertexArray.h"
#include "tiny2d/Vector2.h"
namespace tiny2d {
struct Color { float r=1,g=1,b=1,a=1; };
// Immediate renderer: one draw per sprite, shared immutable quad per renderer.
// Owns blend state during draws (enabled, ADD, RGB SRC_ALPHA/ONE_MINUS_SRC_ALPHA, alpha ONE/ONE_MINUS_SRC_ALPHA),
// leaves program, VAO and texture unit 0 bound. Caller owns depth/scissor/cull state.
class SpriteRenderer {
public:
    SpriteRenderer();
    SpriteRenderer(const SpriteRenderer&)=delete;
    SpriteRenderer& operator=(const SpriteRenderer&)=delete;
    void beginFrame(const Camera2D& camera);
    void draw(const Texture2D& texture,Vector2 position,Vector2 size,float radians=0,Color tint={});
    unsigned drawCalls() const { return static_cast<unsigned>(stats_.drawCalls); }
    const RenderStats& stats() const { return stats_; }
private:
    ShaderProgram program_;
    VertexBuffer buffer_;
    VertexArray array_;
    Mat4 viewProjection_=Mat4::identity();
    RenderStats stats_;
    int modelLocation_=-1,tintLocation_=-1;
};
}
