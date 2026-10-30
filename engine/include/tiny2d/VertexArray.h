#pragma once

#include "tiny2d/detail/GLObject.h"
#include <cstddef>
#include <initializer_list>

namespace tiny2d {
class VertexBuffer;
struct VertexAttribute {
    unsigned int location;
    int components; // 1..4 floats, not normalized
    std::size_t offsetBytes;
};

// Captures one interleaved float layout once. Borrows VBO storage: keep the VBO
// alive until this VAO dies. Same main-thread/current-context contract as VBO.
class VertexArray {
public:
    VertexArray(const VertexBuffer& buffer, std::size_t strideBytes,
                std::initializer_list<VertexAttribute> attributes);
    ~VertexArray() = default;
    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;
    VertexArray(VertexArray&&) noexcept = default;
    VertexArray& operator=(VertexArray&&) noexcept = default;
    void bind() const;
    static void unbind();
    // Requires a linked program bound; binds this VAO and leaves it bound.
    void drawTriangles(int count, int first = 0) const;
    unsigned int nativeHandle() const noexcept { return object_.id(); }
private:
    detail::GLObject object_;
    std::size_t vertexCount_;
};
}
