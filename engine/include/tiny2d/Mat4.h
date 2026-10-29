#pragma once
#include <array>
namespace tiny2d {
// Column-major, column vectors; angles in radians. Positive Z rotation is
// clockwise when viewed in the engine's Y-down screen coordinates.
struct Mat4 {
    std::array<float,16> values{};
    static Mat4 identity();
    static Mat4 translation(float x, float y);
    static Mat4 scale(float x, float y);
    static Mat4 rotationZ(float radians);
    static Mat4 orthographic(float left, float right, float top, float bottom);
    Mat4 operator*(const Mat4& rhs) const;
    const float* data() const { return values.data(); }
};
}
