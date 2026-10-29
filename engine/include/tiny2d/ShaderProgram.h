#pragma once

#include "tiny2d/detail/GLObject.h"
#include <string_view>

namespace tiny2d {

// All operations and destruction require the creating context current on the
// main thread. The context must outlive the object, including moved owners.
class ShaderProgram {
public:
    ShaderProgram(std::string_view vertexSource, std::string_view fragmentSource,
                  std::string_view vertexLabel = "vertex shader",
                  std::string_view fragmentLabel = "fragment shader");
    ~ShaderProgram() = default;
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&&) noexcept = default;
    ShaderProgram& operator=(ShaderProgram&&) noexcept = default;

    void bind() const;
    static void unbind();
    // Requires this program bound; inactive/missing uniforms are errors.
    void setFloat(const char* name, float value) const;
    void setVec2(const char* name, float x, float y) const;
    unsigned int nativeHandle() const noexcept { return object_.id(); }
private:
    int uniformLocation(const char* name) const;
    detail::GLObject object_;
};
}
