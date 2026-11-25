#include "Capture.h"
#include "OpenGL.h"
#include <png.h>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <unistd.h>
#include <vector>

namespace lab {
namespace {
// CLI-only capture of the final back buffer, outside sampling, before swap.
// The application owns this context's default read binding and pixel-pack state.
void writeCapture(const std::string& path,tiny2d::Size size,std::FILE* stream) {
    std::vector<png_byte> pixels(std::size_t(size.width)*size.height*4);
    glReadBuffer(GL_BACK);
    glReadPixels(0,0,size.width,size.height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    tiny2d::detail::checkGL("Lab capture");
    png_image image{};
    image.version=PNG_IMAGE_VERSION;
    image.width=size.width; image.height=size.height; image.format=PNG_FORMAT_RGBA;
    const auto ok=stream ? png_image_write_to_stdio(&image,stream,0,pixels.data(),-size.width*4,nullptr)
                         : png_image_write_to_file(&image,path.c_str(),0,pixels.data(),-size.width*4,nullptr);
    if (!ok) throw std::runtime_error("PNG capture "+path+": "+image.message);
}
}

void capture(const std::string& path,tiny2d::Size size) {
    writeCapture(path,size,nullptr);
}

void captureNew(const std::string& path,tiny2d::Size size) {
    namespace fs=std::filesystem;
    auto temporary=(fs::path(path).parent_path()/".survivor-capture-XXXXXX").string();
    const int fd=::mkstemp(temporary.data());
    if (fd<0) throw std::system_error(errno,std::generic_category(),"stage screenshot "+path);
    struct Cleanup { const char* path; ~Cleanup() { ::unlink(path); } } cleanup{temporary.c_str()};
    // Encode through the exclusive descriptor. Reopening the temporary name
    // for truncation would allow another directory entry to be substituted.
    std::unique_ptr<std::FILE,decltype(&std::fclose)> stream(::fdopen(fd,"wb"),std::fclose);
    if (!stream) {
        const auto error=errno;
        ::close(fd);
        throw std::system_error(error,std::generic_category(),"open screenshot stream "+path);
    }
    writeCapture(path,size,stream.get());
    if (std::fclose(stream.release())!=0)
        throw std::system_error(errno,std::generic_category(),"close screenshot "+path);
    fs::create_hard_link(temporary,path);
}
}
