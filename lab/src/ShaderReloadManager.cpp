#include "ShaderReloadManager.h"
#include "OpenGL.h"
#include "ShaderAssets.h"

#include <stdexcept>
#include <utility>
#include <vector>

namespace lab {
namespace {
// GL 3.3 reflection is independent of the bound program. Check the driver's
// linked interface, including optimization, array shape and uniform blocks.
void validateGalleryProgram(GLuint program) {
    GLint count = 0, maxLength = 0;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);
    glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLength);
    tiny2d::detail::checkGL("gallery uniform reflection");
    std::vector<char> name(maxLength);
    bool hasTime = false, hasResolution = false;
    for (GLint i = 0; i < count; ++i) {
        GLsizei length = 0;
        GLint size = 0;
        GLenum type = 0;
        glGetActiveUniform(program, i, maxLength, &length, &size, &type, name.data());
        tiny2d::detail::checkGL("glGetActiveUniform");
        const std::string uniform(name.data(), static_cast<std::size_t>(length));
        GLenum expected;
        if (uniform == "uTime") {
            hasTime = true;
            expected = GL_FLOAT;
        } else if (uniform == "uResolution") {
            hasResolution = true;
            expected = GL_FLOAT_VEC2;
        } else {
            throw std::runtime_error("Unexpected active gallery uniform: " + uniform);
        }
        if (size != 1 || type != expected || glGetUniformLocation(program, name.data()) < 0)
            throw std::runtime_error("Gallery uniform requires standalone " +
                                     std::string(expected == GL_FLOAT ? "float " : "vec2 ") + uniform);
    }
    if (!hasTime || !hasResolution)
        throw std::runtime_error(std::string("Missing active gallery uniform: ") +
                                 (!hasTime ? "uTime" : "uResolution"));

    // The renderer supplies an empty VAO and draws a gl_VertexID triangle.
    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &count);
    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxLength);
    tiny2d::detail::checkGL("gallery vertex reflection");
    name.resize(maxLength);
    for (GLint i = 0; i < count; ++i) {
        GLsizei length = 0;
        GLint size = 0;
        GLenum type = 0;
        glGetActiveAttrib(program, i, maxLength, &length, &size, &type, name.data());
        tiny2d::detail::checkGL("glGetActiveAttrib");
        const std::string attribute(name.data(), static_cast<std::size_t>(length));
        if (attribute.compare(0, 3, "gl_") != 0)
            throw std::runtime_error("Unexpected gallery vertex attribute: " + attribute);
    }
    const GLint color = glGetFragDataLocation(program, "fragColor");
    tiny2d::detail::checkGL("glGetFragDataLocation");
    if (color != 0) throw std::runtime_error("Gallery requires fragColor at color attachment 0");
}
}

ShaderReloadManager::ShaderReloadManager(std::filesystem::path vertexPath,
                                         std::filesystem::path fragmentPath,std::optional<Sources> startupFallback)
    : vertexPath_(std::move(vertexPath)), fragmentPath_(std::move(fragmentPath)) {
    tiny2d::detail::requireContext("ShaderReloadManager initialization");
    context_ = glfwGetCurrentContext();
    if (!vertexPath_.is_absolute() || !fragmentPath_.is_absolute())
        throw std::invalid_argument("Gallery shader paths must be absolute");
    if (reload()) return;
    const auto editError=status_.error;
    if (startupFallback) {
        vertexPath_.swap(startupFallback->first);
        fragmentPath_.swap(startupFallback->second);
        const bool loaded=reload();
        vertexPath_.swap(startupFallback->first);
        fragmentPath_.swap(startupFallback->second);
        if (loaded) {
            status_.success=false;
            status_.error="External shader rejected; bundled version active. "+editError;
            return;
        }
    }
    throw std::runtime_error("Gallery initialization ["+vertexPath_.string()+" + "+fragmentPath_.string()+"]: "+editError);
}

bool ShaderReloadManager::reload() {
    tiny2d::detail::requireContext("ShaderReloadManager::reload");
    if (glfwGetCurrentContext() != context_)
        throw std::logic_error("ShaderReloadManager::reload requires its owning context");
    try {
        const auto vertex = readShaderFile(vertexPath_);
        const auto fragment = readShaderFile(fragmentPath_);
        tiny2d::ShaderProgram candidate(vertex, fragment, vertexPath_.string(), fragmentPath_.string());
        validateGalleryProgram(candidate.nativeHandle());

        // An external binding survives the entire transaction. If a caller
        // was using this exhibit, transfer its binding before retiring the old
        // program (GLObject would otherwise unbind it on destruction).
        GLint bound = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &bound);
        tiny2d::detail::checkGL("gallery reload current program");
        if (active_ && static_cast<GLuint>(bound) == active_->nativeHandle()) {
            candidate.bind();
            tiny2d::detail::checkGL("gallery reload program switch");
        }
        active_.emplace(std::move(candidate)); // noexcept move; commit after validation.
        ++status_.version;
        status_.success = true;
        status_.error.clear();
        return true;
    } catch (const std::exception& error) {
        status_.success = false;
        status_.error = error.what();
        return false;
    }
}
}
