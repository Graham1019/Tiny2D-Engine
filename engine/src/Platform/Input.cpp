#include "tiny2d/Input.h"
#include "tiny2d/Window.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace tiny2d {
bool Input::isKeyPressed(Key key) const {
    return isKeyPressed(static_cast<int>(key));
}
bool Input::isKeyPressed(int keyCode) const {
    if (keyCode < GLFW_KEY_SPACE || keyCode > GLFW_KEY_LAST) {
        return false;
    }
    return glfwGetKey(window_.handle_.get(), keyCode) == GLFW_PRESS;
}
}
