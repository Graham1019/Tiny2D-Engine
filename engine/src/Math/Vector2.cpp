
#include "tiny2d/Vector2.h"

namespace tiny2d {

Vector2::Vector2()
    : x(0.0f), y(0.0f) {
}

// TODO: 实现带两个参数的构造函数
Vector2::Vector2(float initialX, float initialY) {

    // 请你自己完成：
    // 将 initialX 赋值给 x
    x = initialX;
    // 将 initialY 赋值给 y
    y = initialY;


}

Vector2 Vector2::operator+(const Vector2& other) const {

    // TODO 1:
    // 计算两个向量 x 坐标之和
    float newX = x + other.x;

    // TODO 2:
    // 计算两个向量 y 坐标之和
    float newY = y + other.y;

    // TODO 3:
    // 使用 newX 和 newY 创建一个 Vector2
    // 并将其返回
    return Vector2(newX, newY);
}

}
