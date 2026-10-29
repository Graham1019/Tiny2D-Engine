#include "tiny2d/detail/GLObject.h"
#include "OpenGL.h"

#include <cstdio>
#include <exception>
#include <pthread.h>
#include <stdexcept>
#include <string>
#include <utility>

namespace tiny2d::detail {
namespace {
void destroy(ObjectKind kind, GLuint id) noexcept {
    switch (kind) {
    case ObjectKind::Program: {
        // A current program is otherwise only flagged for deferred deletion.
        GLint current = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &current);
        if (static_cast<GLuint>(current) == id) glUseProgram(0);
        glDeleteProgram(id);
        break;
    }
    case ObjectKind::VertexShader:
    case ObjectKind::FragmentShader: glDeleteShader(id); break;
    case ObjectKind::Texture: glDeleteTextures(1, &id); break;
    case ObjectKind::Buffer: glDeleteBuffers(1, &id); break;
    case ObjectKind::VertexArray: glDeleteVertexArrays(1, &id); break;
    }
}
}

void requireContext(const char* operation) {
    if (!pthread_main_np() || !glfwGetCurrentContext()) {
        throw std::logic_error(std::string(operation) + ": requires the main thread and a current OpenGL context");
    }
}

void checkGL(const char* operation) {
    std::string errors;
    for (GLenum error; (error = glGetError()) != GL_NO_ERROR;) {
        if (!errors.empty()) errors += ", ";
        errors += std::to_string(error);
    }
    if (!errors.empty()) throw std::runtime_error(std::string(operation) + ": OpenGL error(s) " + errors);
}

GLObject::GLObject(ObjectKind kind) : kind_(kind) {
    requireContext("GPU object creation");
    checkGL("before GPU object creation");
    context_ = glfwGetCurrentContext();
    switch (kind_) {
    case ObjectKind::Program: id_ = glCreateProgram(); break;
    case ObjectKind::VertexShader: id_ = glCreateShader(GL_VERTEX_SHADER); break;
    case ObjectKind::FragmentShader: id_ = glCreateShader(GL_FRAGMENT_SHADER); break;
    case ObjectKind::Texture: glGenTextures(1, &id_); break;
    case ObjectKind::Buffer: glGenBuffers(1, &id_); break;
    case ObjectKind::VertexArray: glGenVertexArrays(1, &id_); break;
    }
    try {
        checkGL("GPU object creation");
        if (!id_) throw std::runtime_error("GPU object creation returned zero");
    } catch (...) {
        reset();
        throw;
    }
}

GLObject::~GLObject() noexcept { reset(); }
GLObject::GLObject(GLObject&& other) noexcept
    : kind_(other.kind_), id_(std::exchange(other.id_, 0)),
      context_(std::exchange(other.context_, nullptr)) {}
GLObject& GLObject::operator=(GLObject&& other) noexcept {
    if (this != &other) {
        reset();
        kind_ = other.kind_;
        id_ = std::exchange(other.id_, 0);
        context_ = std::exchange(other.context_, nullptr);
    }
    return *this;
}

void GLObject::requireCurrent(const char* operation) const {
    requireContext(operation);
    if (!id_ || glfwGetCurrentContext() != context_) {
        throw std::logic_error(std::string(operation) + ": moved-from object or wrong current context");
    }
    checkGL(operation);
}

void GLObject::reset() noexcept {
    if (!id_) return;
    if (!pthread_main_np() || glfwGetCurrentContext() != context_) {
        std::fputs("Tiny2D: GPU destruction requires its live owning context on the main thread\n", stderr);
        std::terminate(); // A lifetime contract violation cannot be repaired in a destructor.
    }
    destroy(kind_, id_);
    id_ = 0;
    for (GLenum error; (error = glGetError()) != GL_NO_ERROR;) {
        std::fprintf(stderr, "Tiny2D: OpenGL error during GPU destruction: %u\n", error);
    }
}
}
