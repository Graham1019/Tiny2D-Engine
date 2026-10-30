#include "tiny2d/ShaderProgram.h"
#include "OpenGL.h"

#include <limits>
#include <stdexcept>
#include <string>

namespace tiny2d {
namespace {
std::string infoLog(GLuint id, bool program) {
    GLint length = 0;
    if (program) glGetProgramiv(id, GL_INFO_LOG_LENGTH, &length);
    else glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
    std::string log(length > 0 ? static_cast<std::size_t>(length) : 1, '\0');
    GLsizei written = 0;
    if (program) glGetProgramInfoLog(id, static_cast<GLsizei>(log.size()), &written, log.data());
    else glGetShaderInfoLog(id, static_cast<GLsizei>(log.size()), &written, log.data());
    detail::checkGL("shader/program info log");
    log.resize(static_cast<std::size_t>(written));
    return log.empty() ? "driver returned no info log" : log;
}

detail::GLObject compile(std::string_view source, detail::ObjectKind kind, std::string_view label) {
    if (source.empty() || source.size() > static_cast<std::size_t>(std::numeric_limits<GLint>::max())) {
        throw std::invalid_argument(std::string(label) + ": empty or oversized shader source");
    }
    detail::GLObject shader(kind);
    const char* text = source.data();
    const GLint length = static_cast<GLint>(source.size());
    glShaderSource(shader.id(), 1, &text, &length);
    glCompileShader(shader.id());
    detail::checkGL("glShaderSource/glCompileShader");
    GLint success = GL_FALSE;
    glGetShaderiv(shader.id(), GL_COMPILE_STATUS, &success);
    detail::checkGL("GL_COMPILE_STATUS");
    if (!success) throw std::runtime_error(std::string(label) + ": compilation failed\n" + infoLog(shader.id(), false));
    return shader;
}
}

ShaderProgram::ShaderProgram(std::string_view vertexSource, std::string_view fragmentSource,
                             std::string_view vertexLabel, std::string_view fragmentLabel)
    : object_(detail::ObjectKind::Program) {
    auto vertex = compile(vertexSource, detail::ObjectKind::VertexShader, vertexLabel);
    auto fragment = compile(fragmentSource, detail::ObjectKind::FragmentShader, fragmentLabel);
    glAttachShader(object_.id(), vertex.id());
    glAttachShader(object_.id(), fragment.id());
    glLinkProgram(object_.id());
    detail::checkGL("glAttachShader/glLinkProgram");
    GLint success = GL_FALSE;
    glGetProgramiv(object_.id(), GL_LINK_STATUS, &success);
    detail::checkGL("GL_LINK_STATUS");
    if (!success) {
        throw std::runtime_error("Program link failed [" + std::string(vertexLabel) + " + " +
                                 std::string(fragmentLabel) + "]\n" + infoLog(object_.id(), true));
    }
    glDetachShader(object_.id(), vertex.id());
    glDetachShader(object_.id(), fragment.id());
    detail::checkGL("glDetachShader");
    // Local shader owners delete the now-detached compilation objects.
}

void ShaderProgram::bind() const {
    object_.requireCurrent("ShaderProgram::bind");
    glUseProgram(object_.id());
    detail::checkGL("glUseProgram");
}
void ShaderProgram::unbind() {
    detail::requireContext("ShaderProgram::unbind");
    detail::checkGL("before glUseProgram(0)");
    glUseProgram(0);
    detail::checkGL("glUseProgram(0)");
}
int ShaderProgram::uniformLocation(const char* name) const {
    object_.requireCurrent("uniform update");
    if (!name || !*name) throw std::invalid_argument("uniform name must not be empty");
    GLint current = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &current);
    detail::checkGL("GL_CURRENT_PROGRAM");
    if (static_cast<GLuint>(current) != object_.id()) throw std::logic_error("uniform update requires this program bound");
    const GLint location = glGetUniformLocation(object_.id(), name);
    detail::checkGL("glGetUniformLocation");
    if (location < 0) throw std::invalid_argument(std::string("inactive or missing uniform: ") + name);
    return location;
}
void ShaderProgram::setFloat(const char* name, float value) const {
    glUniform1f(uniformLocation(name), value);
    detail::checkGL("glUniform1f");
}
void ShaderProgram::setVec2(const char* name, float x, float y) const {
    glUniform2f(uniformLocation(name), x, y);
    detail::checkGL("glUniform2f");
}
}
