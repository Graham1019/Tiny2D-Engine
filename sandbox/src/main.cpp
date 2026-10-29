#include "tiny2d/Application.h"

#include <chrono>
#include <exception>
#include <iostream>
#include <string>

namespace {
class Sandbox final : public tiny2d::Application {
public:
    Sandbox() : Application(800, 600, "Tiny2D M1 | WASD: colors | Esc: quit") {}
private:
    void update(float deltaTime) override {
        (void)deltaTime; // Instant color feedback does not require integration.
        if (input().isKeyPressed(tiny2d::Key::Escape)) {
            window().requestClose();
        }
        red_ = input().isKeyPressed(tiny2d::Key::A) ? 0.9f : 0.12f;
        green_ = input().isKeyPressed(tiny2d::Key::W) ? 0.9f : 0.18f;
        blue_ = input().isKeyPressed(tiny2d::Key::D) ? 0.9f : 0.28f;
        if (input().isKeyPressed(tiny2d::Key::S)) {
            red_ = green_ = blue_ = 0.05f;
        }
    }
    void render() override {
        window().clear(red_, green_, blue_);
        ++frames_;
        const auto now = Clock::now();
        const double seconds = std::chrono::duration<double>(now - fpsStart_).count();
        if (seconds >= 1.0) {
            window().setTitle("Tiny2D M1 | FPS: " +
                std::to_string(static_cast<int>(frames_ / seconds)) +
                " | WASD: colors | Esc: quit");
            frames_ = 0;
            fpsStart_ = now;
        }
    }
    using Clock = std::chrono::steady_clock;
    Clock::time_point fpsStart_ = Clock::now();
    unsigned frames_ = 0;
    float red_ = 0.12f, green_ = 0.18f, blue_ = 0.28f;
};
}

int main() {
    try {
        Sandbox sandbox;
        sandbox.run();
    } catch (const std::exception& error) {
        std::cerr << "Tiny2D Sandbox: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
