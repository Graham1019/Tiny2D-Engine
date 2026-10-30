#include "tiny2d/SpriteRenderer.h"
#include "OpenGL.h"
#include <cmath>
#include <stdexcept>
namespace tiny2d {
namespace {
constexpr float quad[]={0,0,0,0, 1,0,1,0, 1,1,1,1, 0,0,0,0, 1,1,1,1, 0,1,0,1};
constexpr auto vertex=R"(#version 330 core
layout(location=0) in vec2 position;
layout(location=1) in vec2 uv;
uniform mat4 uModel;
uniform mat4 uViewProjection;
out vec2 vUV;
void main(){ vUV=uv; gl_Position=uViewProjection*uModel*vec4(position,0,1); }
)";
constexpr auto fragment=R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTexture;
uniform vec4 uTint;
out vec4 color;
void main(){ color=texture(uTexture,vUV)*uTint; }
)";
}
SpriteRenderer::SpriteRenderer():program_(vertex,fragment,"sprite.vert","sprite.frag"),buffer_(quad,sizeof(quad)),array_(buffer_,4*sizeof(float),{{0,2,0},{1,2,2*sizeof(float)}}) {}
void SpriteRenderer::beginFrame(const Camera2D& camera) { viewProjection_=camera.viewProjectionMatrix(); drawCalls_=0; }
void SpriteRenderer::draw(const Texture2D& texture,Vector2 p,Vector2 s,float a,Color tint) {
    if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(s.x)||!std::isfinite(s.y)||s.x<=0||s.y<=0||!std::isfinite(a)||!std::isfinite(tint.r)||!std::isfinite(tint.g)||!std::isfinite(tint.b)||!std::isfinite(tint.a)) throw std::invalid_argument("invalid sprite transform or tint");
    program_.bind();
    glEnable(GL_BLEND); glBlendEquation(GL_FUNC_ADD); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    const auto model=Mat4::translation(p.x+s.x/2,p.y+s.y/2)*Mat4::rotationZ(a)*Mat4::scale(s.x,s.y)*Mat4::translation(-0.5f,-0.5f);
    program_.setMat4("uModel",model); program_.setMat4("uViewProjection",viewProjection_);
    program_.setInt("uTexture",0); program_.setVec4("uTint",tint.r,tint.g,tint.b,tint.a);
    texture.bind(); array_.drawTriangles(6); ++drawCalls_;
}
}
