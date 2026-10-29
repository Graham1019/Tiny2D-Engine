#include "tiny2d/Application.h"
#include "tiny2d/ShaderProgram.h"
#include "tiny2d/VertexArray.h"
#include "tiny2d/VertexBuffer.h"
#include "ShaderAssets.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {
struct Vertex {
    float position[2];
    float color[3];
};
static_assert(std::is_standard_layout_v<Vertex> && sizeof(Vertex) == 5 * sizeof(float));
constexpr Vertex vertices[] = {
    {{-0.65f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.65f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.0f,   0.65f}, {0.0f, 0.0f, 1.0f}},
};

class Sandbox final : public tiny2d::Application {
public:
    explicit Sandbox(unsigned frameLimit)
        : Application(800, 600, "Tiny2D M2 | WASD: colors | Esc: quit"),
          program_(readShaderAsset("triangle.vert"), readShaderAsset("triangle.frag"),
                   "assets/shaders/triangle.vert", "assets/shaders/triangle.frag"),
          buffer_(vertices, sizeof(vertices)),
          array_(buffer_, sizeof(Vertex), {{0, 2, offsetof(Vertex, position)},
                                           {1, 3, offsetof(Vertex, color)}}),
          frameLimit_(frameLimit) {}
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
        const auto framebuffer = window().framebufferSize();
        if (framebuffer.width > 0 && framebuffer.height > 0) {
            // Scale in framebuffer pixels to preserve proportions on resize/Retina.
            const float side = static_cast<float>(std::min(framebuffer.width, framebuffer.height));
            program_.bind();
            program_.setVec2("uScale", side / framebuffer.width, side / framebuffer.height);
            array_.drawTriangles(3);
        }
        if (frameLimit_ && ++totalFrames_ >= frameLimit_) window().requestClose();
        ++frames_;
        const auto now = Clock::now();
        const double seconds = std::chrono::duration<double>(now - fpsStart_).count();
        if (seconds >= 1.0) {
            window().setTitle("Tiny2D M2 | FPS: " +
                std::to_string(static_cast<int>(frames_ / seconds)) +
                " | WASD: colors | Esc: quit");
            frames_ = 0;
            fpsStart_ = now;
        }
    }
    // Reverse destruction: VAO -> VBO -> program -> Application's Window.
    tiny2d::ShaderProgram program_;
    tiny2d::VertexBuffer buffer_;
    tiny2d::VertexArray array_;
    unsigned frameLimit_ = 0;
    unsigned totalFrames_ = 0;
    using Clock = std::chrono::steady_clock;
    Clock::time_point fpsStart_ = Clock::now();
    unsigned frames_ = 0;
    float red_ = 0.12f, green_ = 0.18f, blue_ = 0.28f;
};
}

int main(int argc, char** argv) {
    try {
        unsigned frameLimit = 0;
        if (argc != 1) {
            if (argc != 3 || std::string(argv[1]) != "--frames") {
                throw std::invalid_argument("Usage: tiny2d_sandbox [--frames positive-count]");
            }
            const std::string count = argv[2];
            if (count.empty() || count.find_first_not_of("0123456789") != std::string::npos) {
                throw std::invalid_argument("--frames requires a positive integer");
            }
            const auto parsed = std::stoul(count);
            if (parsed == 0 || parsed > 1000000) throw std::invalid_argument("--frames must be in [1, 1000000]");
            frameLimit = static_cast<unsigned>(parsed);
        }
        Sandbox sandbox(frameLimit);
        sandbox.run();
    } catch (const std::exception& error) {
        std::cerr << "Tiny2D Sandbox: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
