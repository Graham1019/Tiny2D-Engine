#pragma once
#include "tiny2d/Vector2.h"
#include <cmath>
#include <stdexcept>
namespace tiny2d {
struct Transform2D {
    Vector2 position{}, scale{1,1};
    float rotation=0;
    void validate() const {
        if(!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(scale.x)||!std::isfinite(scale.y)||!std::isfinite(rotation)||scale.x<=0||scale.y<=0)
            throw std::invalid_argument("invalid Transform2D");
    }
};
}
