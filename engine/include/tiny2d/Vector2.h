
#pragma once

namespace tiny2d {

class Vector2 {
public:
    float x;
    float y;

    Vector2();
    Vector2(float initialX, float initialY);

    // 向量加法
    Vector2 operator+(const Vector2& other) const;
};

}
