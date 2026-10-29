#pragma once

#include "tiny2d/Input.h"
#include "tiny2d/Window.h"

namespace tiny2d {

class Application {
public:
    Application(int width, int height, const std::string& title);
    virtual ~Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    void run();
protected:
    virtual void update(float deltaTime) = 0;
    virtual void render() = 0;
    Window& window() { return window_; }
    const Input& input() const { return input_; }
private:
    Window window_;
    Input input_;
};
}
