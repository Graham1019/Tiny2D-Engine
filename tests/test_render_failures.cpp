// Deterministic failure injection: production wrappers, fake GL entry points.
// This checks ownership on hard-to-trigger allocation errors, not driver behavior.
#include "tiny2d/ShaderProgram.h"
#include "tiny2d/VertexArray.h"
#include "tiny2d/VertexBuffer.h"
#include "../engine/src/Renderer/OpenGL.h"

#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace {
struct Object { bool deleted = false; std::set<GLuint> shaders; };
std::map<GLuint, Object> live;
GLuint nextId = 1;
GLuint currentProgram = 0;
GLenum pendingError = GL_NO_ERROR;
enum class Fault { None, Program, VertexShader, FragmentShader, Buffer, Array, Upload, Layout, Compile, Link };
Fault fault = Fault::None;
bool returnHandleOnFailure = false;
GLuint create(Fault point) {
    if (fault == point) {
        if (!returnHandleOnFailure) return 0;
        pendingError = GL_OUT_OF_MEMORY;
    }
    const auto id = nextId++;
    live.emplace(id, Object{});
    return id;
}
void collectShaders() {
    for (auto it = live.begin(); it != live.end();) {
        bool attached = false;
        for (const auto& entry : live) attached |= entry.second.shaders.count(it->first) != 0;
        if (it->second.deleted && !attached) it = live.erase(it);
        else ++it;
    }
}
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class F> void fails(F operation) {
    bool caught = false;
    try { operation(); } catch (const std::exception& error) {
        caught = true;
        require(std::strlen(error.what()) > 0, "missing diagnostic");
    }
    require(caught, "expected failure");
    require(live.empty(), "GPU object leaked on failure");
    require(pendingError == GL_NO_ERROR, "error not reported/consumed");
}
constexpr float vertices[15]{};
}

extern "C" {
GLFWwindow* glfwGetCurrentContext(void) { return reinterpret_cast<GLFWwindow*>(1); }
GLenum glGetError(void) { return std::exchange(pendingError, GL_NO_ERROR); }
GLuint glCreateProgram(void) { return create(Fault::Program); }
GLuint glCreateShader(GLenum type) { return create(type == GL_VERTEX_SHADER ? Fault::VertexShader : Fault::FragmentShader); }
void glDeleteProgram(GLuint id) { live.erase(id); collectShaders(); }
void glDeleteShader(GLuint id) { live.at(id).deleted = true; collectShaders(); }
void glGenBuffers(GLsizei, GLuint* id) { *id = create(Fault::Buffer); }
void glGenVertexArrays(GLsizei, GLuint* id) { *id = create(Fault::Array); }
void glDeleteBuffers(GLsizei, const GLuint* id) { live.erase(*id); }
void glDeleteVertexArrays(GLsizei, const GLuint* id) { live.erase(*id); }
void glGetIntegerv(GLenum name, GLint* value) { *value = name == GL_CURRENT_PROGRAM ? currentProgram : 16; }
void glUseProgram(GLuint id) { currentProgram = id; }
void glShaderSource(GLuint, GLsizei, const GLchar* const*, const GLint*) {}
void glCompileShader(GLuint) {}
void glGetShaderiv(GLuint, GLenum name, GLint* value) { *value = name == GL_COMPILE_STATUS ? fault != Fault::Compile : 10; }
void glGetProgramiv(GLuint, GLenum name, GLint* value) { *value = name == GL_LINK_STATUS ? fault != Fault::Link : 10; }
void glGetShaderInfoLog(GLuint, GLsizei, GLsizei* length, GLchar* log) { std::strcpy(log, "injected"); *length = 8; }
void glGetProgramInfoLog(GLuint, GLsizei, GLsizei* length, GLchar* log) { std::strcpy(log, "injected"); *length = 8; }
void glAttachShader(GLuint program, GLuint shader) { live.at(program).shaders.insert(shader); }
void glDetachShader(GLuint program, GLuint shader) { live.at(program).shaders.erase(shader); collectShaders(); }
void glLinkProgram(GLuint) {}
GLint glGetUniformLocation(GLuint, const GLchar*) { return 0; }
void glUniform1f(GLint, GLfloat) {}
void glUniform2f(GLint, GLfloat, GLfloat) {}
void glBindBuffer(GLenum, GLuint) {}
void glBufferData(GLenum, GLsizeiptr, const void*, GLenum) { if (fault == Fault::Upload) pendingError = GL_OUT_OF_MEMORY; }
void glBindVertexArray(GLuint) {}
void glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) {
    if (fault == Fault::Layout) pendingError = GL_INVALID_OPERATION;
}
void glEnableVertexAttribArray(GLuint) {}
void glDrawArrays(GLenum, GLint, GLsizei) {}
}

int main() {
    static_assert(!std::is_copy_constructible_v<tiny2d::ShaderProgram>);
    static_assert(!std::is_copy_assignable_v<tiny2d::VertexBuffer>);
    static_assert(!std::is_copy_constructible_v<tiny2d::VertexArray>);
    static_assert(std::is_nothrow_move_constructible_v<tiny2d::ShaderProgram>);
    static_assert(std::is_nothrow_move_assignable_v<tiny2d::VertexBuffer>);
    static_assert(std::is_nothrow_move_constructible_v<tiny2d::VertexArray>);
    try {
        for (auto failure : {Fault::Program, Fault::VertexShader, Fault::FragmentShader, Fault::Compile, Fault::Link}) {
            fault = failure;
            fails([] { tiny2d::ShaderProgram program("vertex", "fragment", "test.vert", "test.frag"); });
        }
        for (auto failure : {Fault::Buffer, Fault::Upload}) {
            fault = failure;
            fails([] { tiny2d::VertexBuffer buffer(vertices, sizeof(vertices)); });
        }
        for (auto failure : {Fault::Array, Fault::Layout}) {
            fault = failure;
            fails([] {
                tiny2d::VertexBuffer buffer(vertices, sizeof(vertices));
                tiny2d::VertexArray array(buffer, 5 * sizeof(float), {{0, 2, 0}, {1, 3, 2 * sizeof(float)}});
            });
        }
        // Even if a driver writes a handle and reports an error, it is owned
        // before the status check and must be reclaimed by the constructor.
        returnHandleOnFailure = true;
        for (auto failure : {Fault::Program, Fault::VertexShader, Fault::FragmentShader}) {
            fault = failure;
            fails([] { tiny2d::ShaderProgram program("vertex", "fragment"); });
        }
        fault = Fault::Buffer;
        fails([] { tiny2d::VertexBuffer buffer(vertices, sizeof(vertices)); });
        fault = Fault::Array;
        fails([] {
            tiny2d::VertexBuffer buffer(vertices, sizeof(vertices));
            tiny2d::VertexArray array(buffer, 20, {{0, 2, 0}});
        });
        returnHandleOnFailure = false;
        fault = Fault::None;
        fails([] { tiny2d::VertexBuffer buffer(nullptr, 4); });
        fails([] { tiny2d::ShaderProgram program("", "fragment"); });
        pendingError = GL_INVALID_ENUM;
        fails([] { tiny2d::VertexBuffer buffer(vertices, sizeof(vertices)); });
        {
            tiny2d::ShaderProgram first("vertex", "fragment");
            require(live.size() == 1, "successful link retained shader objects");
            tiny2d::ShaderProgram second("vertex", "fragment");
            first.bind();
            first = std::move(second);
            require(live.size() == 1 && second.nativeHandle() == 0, "move assignment leaked/repeated ownership");
            tiny2d::ShaderProgram third(std::move(first));
            third.bind();
        }
        require(live.empty() && currentProgram == 0, "bound program destruction leaked");
        std::cout << "Injected creation, compile, link, upload and layout failures cleaned up all objects\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
