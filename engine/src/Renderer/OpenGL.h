#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>

namespace tiny2d::detail {
void checkGL(const char* operation);
void requireContext(const char* operation);
}
