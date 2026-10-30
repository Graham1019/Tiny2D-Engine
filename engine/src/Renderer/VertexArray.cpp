#include "tiny2d/VertexArray.h"
#include "tiny2d/VertexBuffer.h"
#include "OpenGL.h"

#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace tiny2d {
VertexArray::VertexArray(const VertexBuffer& buffer, std::size_t strideBytes,
                         std::initializer_list<VertexAttribute> attributes)
    : object_(detail::ObjectKind::VertexArray), vertexCount_(0) {
    if (!strideBytes || strideBytes > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()) ||
        strideBytes % sizeof(float) || buffer.sizeBytes() % strideBytes || attributes.size() == 0) {
        throw std::invalid_argument("VertexArray requires a non-empty, whole interleaved float layout");
    }
    GLint maxAttributes = 0;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxAttributes);
    detail::checkGL("GL_MAX_VERTEX_ATTRIBS");
    std::unordered_set<unsigned int> locations;
    for (const auto& attribute : attributes) {
        if (attribute.components < 1 || attribute.components > 4 ||
            attribute.location >= static_cast<unsigned int>(maxAttributes) ||
            !locations.insert(attribute.location).second || attribute.offsetBytes % sizeof(float) ||
            attribute.offsetBytes > strideBytes ||
            static_cast<std::size_t>(attribute.components) * sizeof(float) > strideBytes - attribute.offsetBytes) {
            throw std::invalid_argument("VertexArray: invalid or duplicate float attribute");
        }
    }
    vertexCount_ = buffer.sizeBytes() / strideBytes;
    bind();
    buffer.bind();
    for (const auto& attribute : attributes) {
        glVertexAttribPointer(attribute.location, attribute.components, GL_FLOAT, GL_FALSE,
                              static_cast<GLsizei>(strideBytes),
                              reinterpret_cast<const void*>(attribute.offsetBytes));
        glEnableVertexAttribArray(attribute.location);
    }
    detail::checkGL("vertex attribute layout");
    unbind();
    VertexBuffer::unbind(); // VAO retains the buffer association captured above.
}
void VertexArray::bind() const {
    object_.requireCurrent("VertexArray::bind");
    glBindVertexArray(object_.id());
    detail::checkGL("glBindVertexArray");
}
void VertexArray::unbind() {
    detail::requireContext("VertexArray::unbind");
    detail::checkGL("before glBindVertexArray(0)");
    glBindVertexArray(0);
    detail::checkGL("glBindVertexArray(0)");
}
void VertexArray::drawTriangles(int count, int first) const {
    if (count < 0 || first < 0 || static_cast<std::size_t>(first) > vertexCount_ ||
        static_cast<std::size_t>(count) > vertexCount_ - static_cast<std::size_t>(first)) {
        throw std::out_of_range("VertexArray::drawTriangles exceeds uploaded vertex data");
    }
    bind();
    GLint program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    detail::checkGL("draw program check");
    if (!program) throw std::logic_error("drawTriangles requires a bound ShaderProgram");
    glDrawArrays(GL_TRIANGLES, first, count);
    detail::checkGL("glDrawArrays(GL_TRIANGLES)");
}
}
