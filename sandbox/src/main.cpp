#include "tiny2d/Application.h"
#include "tiny2d/ShaderProgram.h"
#include "tiny2d/VertexArray.h"
#include "tiny2d/VertexBuffer.h"
#include "ShaderAssets.h"
#include "tiny2d/SpriteRenderer.h"
#include "tiny2d/BatchRenderer.h"
#include <memory>
#include <vector>
#include <cmath>

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
    explicit Sandbox(unsigned frameLimit, bool sprites, unsigned count, bool batch, bool multi)
        : Application(800, 600, "Tiny2D M4 | Arrows: camera | Q/E: zoom | WASD: colors | Esc: quit"),
          program_(readShaderAsset("triangle.vert"), readShaderAsset("triangle.frag"),
                   "assets/shaders/triangle.vert", "assets/shaders/triangle.frag"),
          buffer_(vertices, sizeof(vertices)),
          array_(buffer_, sizeof(Vertex), {{0, 2, offsetof(Vertex, position)},
                                           {1, 3, offsetof(Vertex, color)}}),
          frameLimit_(frameLimit), sprites_(sprites), count_(count), batchMode_(batch), multi_(multi) {
        if(sprites_) {
            texture_=std::make_unique<tiny2d::Texture2D>(
                tiny2d::ImageLoader::loadPNG(assetPath("textures/sprite.png")));
            if(batchMode_) batch_=std::make_unique<tiny2d::BatchRenderer>();
            else renderer_=std::make_unique<tiny2d::SpriteRenderer>();
        }
        if(multi_) for(const char* name:{"red.png","green.png","blue.png"}) textures_.push_back(std::make_unique<tiny2d::Texture2D>(tiny2d::ImageLoader::loadPNG(assetPath((std::string("textures/")+name).c_str()))));
        camera_.setPosition(400,300);
    }
private:
    void update(float deltaTime) override {
        angle_+=deltaTime*0.7f;
        camera_.move((input().isKeyPressed(tiny2d::Key::Right)-input().isKeyPressed(tiny2d::Key::Left))*240*deltaTime,
                     (input().isKeyPressed(tiny2d::Key::Down)-input().isKeyPressed(tiny2d::Key::Up))*240*deltaTime);
        camera_.setZoom(std::clamp(camera_.zoom()*std::exp((input().isKeyPressed(tiny2d::Key::E)-input().isKeyPressed(tiny2d::Key::Q))*deltaTime),0.1f,10.0f));
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
        frameDraws_=0;
        window().clear(red_, green_, blue_);
        const auto framebuffer = window().framebufferSize();
        if (framebuffer.width > 0 && framebuffer.height > 0) {
            const auto logical=window().size();
            if(sprites_ && logical.width>0 && logical.height>0) {
                camera_.setViewportSize(logical.width,logical.height);
                if(batchMode_) batch_->beginFrame(camera_);
                else renderer_->beginFrame(camera_);
                for(unsigned i=0;i<count_;++i) {
                    const float x=80+float(i%10)*58, y=80+float((i/10)%50)*48;
                    const auto& texture=multi_?*textures_[i%textures_.size()]:*texture_;
                    if(batchMode_) batch_->draw(texture,{x,y},{110,110},i==0?angle_:0,
                        {i%2?0.4f:1.0f,i%2?1.0f:0.5f,1.0f,i<3?0.65f:1.0f});
                    else renderer_->draw(texture,{x,y},{110,110}, i==0?angle_:0,
                        {i%2?0.4f:1.0f,i%2?1.0f:0.5f,1.0f,i<3?0.65f:1.0f});
                }
                if(batchMode_)batch_->endFrame();
                frameDraws_=batchMode_?batch_->stats().drawCalls:renderer_->drawCalls();
            } else if(!sprites_) {
            // Scale in framebuffer pixels to preserve proportions on resize/Retina.
            const float side = static_cast<float>(std::min(framebuffer.width, framebuffer.height));
            program_.bind();
            program_.setVec2("uScale", side / framebuffer.width, side / framebuffer.height);
            array_.drawTriangles(3);frameDraws_=1;
            }
        }
        // frameDraws_ is the displayed per-frame count, including skipped/minimized frames.
        // Renderer statistics describe its last begun pass; no pass is begun when undrawable.
        if (frameLimit_ && ++totalFrames_ >= frameLimit_) {
            std::cout << "Frames: " << totalFrames_ << " | Draw calls in final frame: "
                      << frameDraws_ << '\n';
            window().requestClose();
        }
        ++frames_;
        const auto now = Clock::now();
        const double seconds = std::chrono::duration<double>(now - fpsStart_).count();
        if (seconds >= 1.0) {
            window().setTitle("Tiny2D M4 | Draws: " + std::to_string(frameDraws_) + " | FPS: " +
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
    tiny2d::Camera2D camera_{800,600};
    std::unique_ptr<tiny2d::Texture2D> texture_;
    std::unique_ptr<tiny2d::SpriteRenderer> renderer_;
    std::unique_ptr<tiny2d::BatchRenderer> batch_;
    std::vector<std::unique_ptr<tiny2d::Texture2D>> textures_;
    bool batchMode_,multi_;
    unsigned frameDraws_=0;
    unsigned frameLimit_ = 0;
    bool sprites_;
    unsigned count_;
    float angle_=0;
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
        bool sprites=true,batch=false,multi=false; unsigned count=12;
        for(int i=1;i<argc;i+=2) {
            if(i+1>=argc) throw std::invalid_argument("missing option value");
            const std::string option=argv[i], value=argv[i+1];
            if(option=="--renderer") {
                if(value!="immediate"&&value!="batch")throw std::invalid_argument("renderer must be immediate or batch");batch=value=="batch";
            } else if(option=="--scene") {
                if(value!="sprites"&&value!="triangle"&&value!="multitexture") throw std::invalid_argument("scene must be sprites or triangle");
                sprites=value!="triangle";multi=value=="multitexture";
            } else if(option=="--frames"||option=="--sprite-count") {
                if(value.empty()||value.find_first_not_of("0123456789")!=std::string::npos) throw std::invalid_argument("count must be a positive integer");
                const auto n=std::stoul(value);
                if(n>(option=="--frames"?1000000u:100000u)) throw std::invalid_argument("count outside allowed range");
                if(option=="--frames") frameLimit=n; else count=n;
            } else throw std::invalid_argument("Usage: tiny2d_sandbox [--renderer immediate|batch] [--scene sprites|multitexture|triangle] [--frames N] [--sprite-count 0..100000]");
        }
        Sandbox sandbox(frameLimit,sprites,count,batch,multi);
        sandbox.run();
    } catch (const std::exception& error) {
        std::cerr << "Tiny2D Sandbox: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
