#pragma once
#include "Presentation.h"

namespace lab::quad {
using Quads = std::vector<Quad>;
void rect(Quads&,float x,float y,float w,float h,tiny2d::Color,float angle=0);
void segment(Quads&,tiny2d::Vector2 a,tiny2d::Vector2 b,float width,tiny2d::Color);
tiny2d::Vector2 polar(tiny2d::Vector2 center,float radius,float angle);
void ring(Quads&,tiny2d::Vector2 center,float radius,int sides,float angle,float width,tiny2d::Color);
void lettering(Quads&,const std::string&,float x,float y,float scale,tiny2d::Color);
}
