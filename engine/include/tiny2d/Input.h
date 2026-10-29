#pragma once

namespace tiny2d {
class Window;

// Values follow GLFW's keyboard key tokens; other named keys can be added here.
enum class Key : int { A = 65, D = 68, S = 83, W = 87, Escape = 256 };

class Input {
public:
    // Non-owning reference: Window must outlive Input. Main thread only.
    explicit Input(const Window& window) : window_(window) {}
    bool isKeyPressed(Key key) const;
    // Supports all GLFW keyboard tokens, returning false for invalid tokens.
    bool isKeyPressed(int keyCode) const;
private:
    const Window& window_;
};
}
