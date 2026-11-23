#include "tiny2d/TimestampQueries.h"
#include "OpenGL.h"
#include <vector>

namespace tiny2d {
namespace {
class GLTimestamps final : public TimestampQueries {
    GLFWwindow* context_;
    std::vector<GLuint> ids_;
    void requireOwner() const {
        detail::requireContext("timestamp query");
        if (context_!=glfwGetCurrentContext()) throw std::logic_error("timestamp query on foreign context");
    }
public:
    explicit GLTimestamps(unsigned capacity):context_(glfwGetCurrentContext()) {
        requireOwner();
        GLint major=0,minor=0;
        glGetIntegerv(GL_MAJOR_VERSION,&major); glGetIntegerv(GL_MINOR_VERSION,&minor);
        if ((major<3 || (major==3 && minor<3)) && !glfwExtensionSupported("GL_ARB_timer_query")) {
            reason="requires GL 3.3 or ARB_timer_query"; return;
        }
        if (!glfwGetProcAddress("glQueryCounter") || !glfwGetProcAddress("glGetQueryObjectui64v")) {
            reason="timestamp entry point unavailable"; return;
        }
        glGetQueryiv(GL_TIMESTAMP,GL_QUERY_COUNTER_BITS,&bits);
        detail::checkGL("GL_TIMESTAMP counter bits");
        if (bits<=0 || bits>64) { reason="N/A: GL_TIMESTAMP counter bits = "+std::to_string(bits); bits=0; return; }
        ids_.resize(capacity);
        glGenQueries(capacity,ids_.data());
        try { detail::checkGL("allocate timestamp ring"); }
        catch (...) { glDeleteQueries(capacity,ids_.data()); throw; }
        reason="GL_TIMESTAMP / "+std::to_string(bits)+" bits";
    }
    ~GLTimestamps() override {
        if (ids_.empty()) return;
        requireOwner();
        glDeleteQueries(GLsizei(ids_.size()),ids_.data());
    }
    void stamp(unsigned i) override { requireOwner(); glQueryCounter(ids_.at(i),GL_TIMESTAMP); detail::checkGL("timestamp submission"); }
    bool available(unsigned i) override {
        requireOwner(); GLint ready=0;
        glGetQueryObjectiv(ids_.at(i),GL_QUERY_RESULT_AVAILABLE,&ready);
        return ready==GL_TRUE;
    }
    std::uint64_t result(unsigned i) override {
        requireOwner(); GLuint64 value=0;
        glGetQueryObjectui64v(ids_.at(i),GL_QUERY_RESULT,&value);
        return value;
    }
};
}
std::unique_ptr<TimestampQueries> openTimestampQueries(unsigned capacity) {
    return std::make_unique<GLTimestamps>(capacity);
}
}
