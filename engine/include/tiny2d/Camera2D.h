#pragma once
#include "tiny2d/Mat4.h"
namespace tiny2d {
// CPU-only. Position is the world-space center; viewport uses logical pixels.
class Camera2D {
public:
    Camera2D(float width, float height) { setViewportSize(width,height); }
    void setPosition(float x,float y);
    void move(float x,float y) { setPosition(x_+x,y_+y); }
    void setZoom(float zoom);
    float zoom() const { return zoom_; }
    void setViewportSize(float width,float height);
    Mat4 viewProjectionMatrix() const;
private:
    float x_=0,y_=0,width_=1,height_=1,zoom_=1;
};
}
