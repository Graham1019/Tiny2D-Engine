#include "tiny2d/VertexBuffer.h"
#include "OpenGL.h"

#include <limits>
#include <stdexcept>

namespace tiny2d {
VertexBuffer::VertexBuffer(const void* data, std::size_t sizeBytes)
    : object_(detail::ObjectKind::Buffer), sizeBytes_(sizeBytes) {
    if (!data || !sizeBytes || sizeBytes > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max())) {
        throw std::invalid_argument("VertexBuffer requires non-empty data and a representable size");
    }
    bind();
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeBytes), data, GL_STATIC_DRAW);
    detail::checkGL("glBufferData(GL_STATIC_DRAW)");
}
void VertexBuffer::bind() const {
    object_.requireCurrent("VertexBuffer::bind");
    glBindBuffer(GL_ARRAY_BUFFER, object_.id());
    detail::checkGL("glBindBuffer");
}
void VertexBuffer::unbind() {
    detail::requireContext("VertexBuffer::unbind");
    detail::checkGL("before glBindBuffer(0)");
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    detail::checkGL("glBindBuffer(0)");
}
}
