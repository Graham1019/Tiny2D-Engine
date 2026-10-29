
#include <cassert>
#include <iostream>

#include "tiny2d/Vector2.h"

int main() {
    using tiny2d::Vector2;

    // Test 1: Basic vector addition
    const Vector2 a(2.0f, 3.0f);
    const Vector2 b(4.0f, 5.0f);

    Vector2 result = a + b;

    assert(result.x == 6.0f);
    assert(result.y == 8.0f);

    // Test 2: Original vectors remain unchanged
    assert(a.x == 2.0f);
    assert(a.y == 3.0f);
    assert(b.x == 4.0f);
    assert(b.y == 5.0f);

    // Test 3
    // Test addition with negative numbers
    const Vector2 c(-2.0f, -3.0f);
    const Vector2 d(4.0f, 5.0f);

    Vector2 result2 = c + d;

    assert(result2.x == 2.0f);
    assert(result2.y == 2.0f);

    // Test 4
    // Test addition with a zero vector
    const Vector2 zero(0.0f, 0.0f);
    const Vector2 e(1.0f, 2.0f);

    Vector2 result3 = zero + e;

    assert(result3.x == 1.0f);
    assert(result3.y == 2.0f);

    // Test 5
    // Test addition with both vectors being negative
    const Vector2 f(-1.0f, -2.0f);
    const Vector2 g(-3.0f, -4.0f);

    Vector2 result4 = f + g;

    assert(result4.x == -4.0f);
    assert(result4.y == -6.0f);

    std::cout << "All Vector2 tests passed!\n";

    return 0;
}
