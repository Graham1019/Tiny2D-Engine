#pragma once
#include "tiny2d/Scene.h"
#include <string>
namespace tiny2d_ui {
void rectangle(tiny2d::Scene&,float x,float y,float w,float h,float r,float g,float b,float a=1);
void text(tiny2d::Scene&,const std::string&,float x,float y,float scale,float r=1,float g=1,float b=1);
float textWidth(const std::string&,float scale);
}
