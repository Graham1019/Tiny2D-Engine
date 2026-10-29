#include "tiny2d/Vector2.h"

namespace tiny2d {

Vector2::Vector2() : x(0.0f), y(0.0f) {}

Vector2::Vector2(float initialX, float initialY) : x(initialX), y(initialY) {}

Vector2 Vector2::operator+(const Vector2& other) const {
    return Vector2(x + other.x, y + other.y);
}

}
