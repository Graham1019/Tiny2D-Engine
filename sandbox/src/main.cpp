
#include <iostream>
#include "tiny2d/Vector2.h"

int main() {

    tiny2d::Vector2 a(2.0f, 3.0f);
    tiny2d::Vector2 b(4.0f, 5.0f);

    tiny2d::Vector2 c = a + b;

    std::cout << "Result: ("
              << c.x << ", "
              << c.y << ")\n";

    return 0;
}
