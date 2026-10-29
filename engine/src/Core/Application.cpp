#include "tiny2d/Application.h"

#include <algorithm>
#include <chrono>

namespace tiny2d {
Application::Application(int width, int height, const std::string& title)
    : window_(width, height, title), input_(window_) {}

void Application::run() {
    using Clock = std::chrono::steady_clock;
    auto previous = Clock::now();
    while (!window_.shouldClose()) {
        window_.pollEvents();
        if (window_.shouldClose()) {
            break;
        }
        const auto now = Clock::now();
        const float elapsed = std::chrono::duration<float>(now - previous).count();
        previous = now;
        update(std::clamp(elapsed, 0.0f, 0.1f));
        if (window_.shouldClose()) {
            break;
        }
        window_.makeContextCurrent();
        render();
        window_.swapBuffers();
    }
}
}
