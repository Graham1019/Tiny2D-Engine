#include "tiny2d/Application.h"
#include "tiny2d/ShaderProgram.h"
#include "tiny2d/VertexArray.h"
#include "tiny2d/VertexBuffer.h"
#include "ShaderAssets.h"
#include "../engine/src/Renderer/OpenGL.h"

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class F> void fails(F operation, const char* diagnostic) {
    try { operation(); } catch (const std::exception& error) {
        require(std::string(error.what()).find(diagnostic) != std::string::npos, "incorrect error diagnostic");
        std::cout << "Expected: " << error.what() << '\n';
        return;
    }
    throw std::runtime_error("expected operation to fail");
}
constexpr float vertices[] = {
    -0.65f, -0.5f, 1, 0, 0,
     0.65f, -0.5f, 0, 1, 0,
     0.0f,   0.65f, 0, 0, 1,
};
std::array<unsigned char, 4> pixel(tiny2d::Size size, float x, float y) {
    std::array<unsigned char, 4> result{};
    glReadPixels(static_cast<int>((x + 1) * 0.5f * size.width),
                 static_cast<int>((y + 1) * 0.5f * size.height),
                 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
    return result;
}
class ThrowingApplication final : public tiny2d::Application {
public:
    ThrowingApplication() : Application(80, 60, "M2 exception test"),
        program(readShaderAsset("triangle.vert"), readShaderAsset("triangle.frag")),
        buffer(vertices, sizeof(vertices)), array(buffer, 20, {{0, 2, 0}, {1, 3, 8}}) {}
private:
    void update(float) override {}
    void render() override { program.bind(); throw std::runtime_error("render exception propagated"); }
    tiny2d::ShaderProgram program;
    tiny2d::VertexBuffer buffer;
    tiny2d::VertexArray array;
};
}

int main() {
    try {
        tiny2d::Window window(320, 240, "Tiny2D OpenGL tests");
        std::cout << "OpenGL: " << glGetString(GL_VERSION) << " | " << glGetString(GL_RENDERER) << '\n';
        const auto vertex = readShaderAsset("triangle.vert");
        const auto fragment = readShaderAsset("triangle.frag");
        fails([&] { tiny2d::ShaderProgram invalid("#version 330 core\nthis is invalid GLSL", fragment,
                                                "broken.vert", "triangle.frag"); }, "broken.vert: compilation failed");
        fails([&] { tiny2d::ShaderProgram invalid(vertex, "#version 330 core\ninvalid", "triangle.vert", "broken.frag"); },
              "broken.frag: compilation failed");
        fails([&] { tiny2d::ShaderProgram invalid(vertex,
            "#version 330 core\nin vec2 vColor; out vec4 color; void main(){ color=vec4(vColor,0,1); }"); }, "Program link failed");
        GLuint programId = 0, bufferId = 0, arrayId = 0;
        {
            tiny2d::ShaderProgram program(vertex, fragment);
            programId = program.nativeHandle();
            GLint attached = -1;
            glGetProgramiv(programId, GL_ATTACHED_SHADERS, &attached);
            require(attached == 0, "linked program retained shader objects");
            tiny2d::ShaderProgram moved(std::move(program));
            require(program.nativeHandle() == 0, "program move did not empty source");
            fails([&] { program.bind(); }, "moved-from");
            moved.bind();
            moved.setVec2("uScale", 1, 1);
            glfwMakeContextCurrent(nullptr);
            fails([&] { moved.bind(); }, "current OpenGL context");
            window.makeContextCurrent();
            bool rejectedWorker = false;
            std::thread worker([&] {
                try { moved.bind(); } catch (const std::logic_error&) { rejectedWorker = true; }
            });
            worker.join();
            require(rejectedWorker, "GPU use on a worker thread was not rejected");
            fails([&] { moved.setFloat("missing", 1); }, "missing uniform");
            tiny2d::VertexBuffer buffer(vertices, sizeof(vertices));
            bufferId = buffer.nativeHandle();
            tiny2d::VertexBuffer movedBuffer(std::move(buffer));
            require(buffer.nativeHandle() == 0, "buffer move did not empty source");
            tiny2d::VertexArray array(movedBuffer, 20, {{0, 2, 0}, {1, 3, 8}});
            arrayId = array.nativeHandle();
            tiny2d::VertexArray movedArray(std::move(array));
            require(array.nativeHandle() == 0, "array move did not empty source");
            movedArray.bind();
            GLint stride = 0, capturedBuffer = 0;
            glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
            glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &capturedBuffer);
            require(stride == 20 && static_cast<GLuint>(capturedBuffer) == bufferId, "incorrect VAO layout");
            fails([&] { movedArray.drawTriangles(4); }, "exceeds uploaded");
            fails([&] { tiny2d::VertexArray bad(movedBuffer, 20, {{0, 4, 8}}); }, "invalid");
            glReadBuffer(GL_BACK);
            window.clear(0.12f, 0.18f, 0.28f);
            movedArray.drawTriangles(3);
            const auto size = window.framebufferSize();
            const auto center = pixel(size, 0, (-0.5f - 0.5f + 0.65f) / 3);
            for (int channel = 0; channel < 3; ++channel) {
                require(std::abs(static_cast<int>(center[channel]) - 85) < 12, "triangle centroid is not interpolated RGB");
            }
            require(pixel(size, -0.5f, -0.4f)[0] > 190, "red corner missing");
            require(pixel(size,  0.5f, -0.4f)[1] > 190, "green corner missing");
            require(pixel(size,  0.0f,  0.5f)[2] > 190, "blue corner missing");
            require(pixel(size, 0.9f, 0.9f)[0] < 40, "background incorrect");
            window.swapBuffers();
            glfwSetWindowSize(glfwGetCurrentContext(), 480, 300);
            window.pollEvents();
            const auto resized = window.framebufferSize();
            GLint viewport[4]{};
            glGetIntegerv(GL_VIEWPORT, viewport);
            require(viewport[2] == resized.width && viewport[3] == resized.height, "resize viewport mismatch");
            require(glGetError() == GL_NO_ERROR, "OpenGL error after rendering/readback");
        }
        require(!glIsProgram(programId) && !glIsBuffer(bufferId) && !glIsVertexArray(arrayId), "GPU resources survived destruction");
        {
            tiny2d::VertexBuffer first(vertices, sizeof(vertices));
            tiny2d::VertexBuffer second(vertices, sizeof(vertices));
            const auto replacedBuffer = first.nativeHandle();
            first = std::move(second);
            require(!glIsBuffer(replacedBuffer) && second.nativeHandle() == 0, "VBO move assignment leaked");
            tiny2d::VertexArray a(first, 20, {{0, 2, 0}, {1, 3, 8}});
            tiny2d::VertexArray b(first, 20, {{0, 2, 0}, {1, 3, 8}});
            const auto replacedArray = a.nativeHandle();
            a = std::move(b);
            require(!glIsVertexArray(replacedArray) && b.nativeHandle() == 0, "VAO move assignment leaked");
        }
        fails([] { ThrowingApplication app; app.run(); }, "render exception propagated");
        window.makeContextCurrent(); // The nested application's context was destroyed.
        require(glGetError() == GL_NO_ERROR, "OpenGL error after resource lifecycle test");
        window.requestClose();
        require(window.shouldClose(), "close request not recorded");
        std::cout << "Real shader diagnostics, RGB pixel readback, resize, moves, destruction and exception unwinding passed\n";
    } catch (const std::exception& error) {
        std::cerr << "OpenGLResourceTests: " << error.what() << '\n';
        return 1; // No silent skip when no GUI/context is available.
    }
}
