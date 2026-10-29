#pragma once

#include <memory>
#include <string>

struct GLFWwindow;

namespace tiny2d {

class Input;

struct Size {
    int width;
    int height;
};

// All Window operations, including construction/destruction, require the main thread.
class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    bool shouldClose() const;
    void requestClose();
    void pollEvents();
    void swapBuffers();
    void makeContextCurrent();
    Size size() const;
    Size framebufferSize() const;
    void setTitle(const std::string& title);
    void clear(float red, float green, float blue, float alpha = 1.0f);

private:
    friend class Input;
    struct Runtime;
    struct WindowDeleter {
        void operator()(GLFWwindow* window) const noexcept;
    };
    static std::shared_ptr<Runtime> acquireRuntime();
    static void framebufferResized(GLFWwindow* window, int width, int height);

    // Reverse member destruction releases the window before the last GLFW runtime.
    std::shared_ptr<Runtime> runtime_;
    std::unique_ptr<GLFWwindow, WindowDeleter> handle_;
};

}
