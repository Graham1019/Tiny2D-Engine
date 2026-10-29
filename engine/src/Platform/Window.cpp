#include "tiny2d/Window.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

#include <stdexcept>

namespace tiny2d {
namespace {
std::runtime_error glfwError(const char* operation) {
    const char* description = nullptr;
    const int code = glfwGetError(&description);
    return std::runtime_error(std::string(operation) + " (GLFW " +
                              std::to_string(code) + "): " +
                              (description ? description : "unknown error"));
}
}

struct Window::Runtime {
    Runtime() {
        if (glfwInit() != GLFW_TRUE) {
            throw glfwError("GLFW initialization failed");
        }
    }
    ~Runtime() { glfwTerminate(); }
};

std::shared_ptr<Window::Runtime> Window::acquireRuntime() {
    // Main-thread-only weak registry: it never extends GLFW's lifetime.
    static std::weak_ptr<Runtime> active;
    auto runtime = active.lock();
    if (!runtime) {
        runtime = std::make_shared<Runtime>();
        active = runtime;
    }
    return runtime;
}

void Window::WindowDeleter::operator()(GLFWwindow* window) const noexcept {
    glfwDestroyWindow(window);
}

Window::Window(int width, int height, const std::string& title) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Window dimensions must be positive");
    }
    runtime_ = acquireRuntime();
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    handle_.reset(glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr));
    if (!handle_) {
        throw glfwError("Window/OpenGL 3.3 Core context creation failed");
    }
    makeContextCurrent();
    // Future OpenGL function loader initialization belongs here.
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(handle_.get(), framebufferResized);
    const auto dimensions = framebufferSize();
    glViewport(0, 0, dimensions.width, dimensions.height);
}

Window::~Window() = default;

void Window::framebufferResized(GLFWwindow* window, int width, int height) {
    // Callbacks can target a different window from the current context.
    GLFWwindow* previous = glfwGetCurrentContext();
    glfwMakeContextCurrent(window);
    glViewport(0, 0, width, height);
    glfwMakeContextCurrent(previous);
}

bool Window::shouldClose() const { return glfwWindowShouldClose(handle_.get()); }
void Window::requestClose() { glfwSetWindowShouldClose(handle_.get(), GLFW_TRUE); }
void Window::pollEvents() { glfwPollEvents(); }
void Window::makeContextCurrent() { glfwMakeContextCurrent(handle_.get()); }
void Window::swapBuffers() { glfwSwapBuffers(handle_.get()); }
Size Window::size() const {
    Size result{};
    glfwGetWindowSize(handle_.get(), &result.width, &result.height);
    return result;
}
Size Window::framebufferSize() const {
    Size result{};
    glfwGetFramebufferSize(handle_.get(), &result.width, &result.height);
    return result;
}
void Window::setTitle(const std::string& title) {
    glfwSetWindowTitle(handle_.get(), title.c_str());
}
void Window::clear(float red, float green, float blue, float alpha) {
    makeContextCurrent();
    glClearColor(red, green, blue, alpha);
    glClear(GL_COLOR_BUFFER_BIT);
}

}
