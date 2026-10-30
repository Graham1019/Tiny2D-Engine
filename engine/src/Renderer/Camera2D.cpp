#include "tiny2d/Camera2D.h"
#include <cmath>
#include <stdexcept>
namespace tiny2d {
void Camera2D::setPosition(float x,float y) {
    if(!std::isfinite(x)||!std::isfinite(y)) throw std::invalid_argument("invalid camera position");
    x_=x; y_=y;
}
void Camera2D::setZoom(float z) {
    if(!std::isfinite(z)||z<0.0001f||z>10000) throw std::invalid_argument("camera zoom must be in [0.0001,10000]");
    zoom_=z;
}
void Camera2D::setViewportSize(float w,float h) {
    if(!std::isfinite(w)||!std::isfinite(h)||w<1||h<1||w>1000000||h>1000000) throw std::invalid_argument("invalid camera viewport [1,1000000]");
    width_=w; height_=h;
}
Mat4 Camera2D::viewProjectionMatrix() const {
    return Mat4::orthographic(-width_/2,width_/2,-height_/2,height_/2)*Mat4::scale(zoom_,zoom_)*Mat4::translation(-x_,-y_);
}
}
