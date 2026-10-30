#pragma once

#include "tiny2d/detail/GLObject.h"
#include <cstddef>

namespace tiny2d {
// Immutable static upload. Creating context must remain live and current on the
// main thread for every operation/destruction. VAOs borrowing it die first.
class VertexBuffer {
public:
    VertexBuffer(const void* data, std::size_t sizeBytes);
    ~VertexBuffer() = default;
    VertexBuffer(const VertexBuffer&) = delete;
    VertexBuffer& operator=(const VertexBuffer&) = delete;
    VertexBuffer(VertexBuffer&&) noexcept = default;
    VertexBuffer& operator=(VertexBuffer&&) noexcept = default;
    void bind() const;
    static void unbind();
    std::size_t sizeBytes() const noexcept { return sizeBytes_; }
    unsigned int nativeHandle() const noexcept { return object_.id(); }
private:
    detail::GLObject object_;
    std::size_t sizeBytes_;
};
}
