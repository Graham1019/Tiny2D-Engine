#pragma once
#include "tiny2d/SpriteRenderer.h"
#include <array>
#include <cmath>
#include <stdexcept>
namespace tiny2d::detail {
// Shared CPU validation and exact world-vertex calculation for preflight/batch.
inline std::array<Vector2,4> spriteCorners(Vector2 p,Vector2 s,float angle,Color c) {
    if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(s.x)||!std::isfinite(s.y)||s.x<=0||s.y<=0||!std::isfinite(angle)||!std::isfinite(c.r)||!std::isfinite(c.g)||!std::isfinite(c.b)||!std::isfinite(c.a))
        throw std::invalid_argument("invalid sprite geometry or tint");
    const auto model=Mat4::translation(p.x+s.x/2,p.y+s.y/2)*Mat4::rotationZ(angle)*Mat4::scale(s.x,s.y)*Mat4::translation(-.5f,-.5f);
    const float uv[4][2]={{0,0},{1,0},{1,1},{0,1}};
    std::array<Vector2,4> result;
    for(unsigned i=0;i<4;++i) {
        float u=uv[i][0],v=uv[i][1];
        float x=model.values[0]*u+model.values[4]*v+model.values[12];
        float y=model.values[1]*u+model.values[5]*v+model.values[13];
        if(!std::isfinite(x)||!std::isfinite(y))throw std::invalid_argument("transform overflow");
        result[i]={x,y};
    }
    return result;
}
}
