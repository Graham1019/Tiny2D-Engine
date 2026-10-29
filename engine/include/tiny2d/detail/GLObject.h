#pragma once

struct GLFWwindow;

namespace tiny2d::detail {

enum class ObjectKind { Program, VertexShader, FragmentShader, Buffer, VertexArray };

// Internal owner shared by the small rendering wrappers. No public GL headers.
class GLObject {
public:
    explicit GLObject(ObjectKind kind);
    ~GLObject() noexcept;
    GLObject(const GLObject&) = delete;
    GLObject& operator=(const GLObject&) = delete;
    GLObject(GLObject&& other) noexcept;
    GLObject& operator=(GLObject&& other) noexcept;
    unsigned int id() const noexcept { return id_; }
    void requireCurrent(const char* operation) const;
private:
    void reset() noexcept;
    ObjectKind kind_;
    unsigned int id_ = 0;
    GLFWwindow* context_ = nullptr;
};

}
