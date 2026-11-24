#include "Capture.h"
#include "OpenGL.h"
#include <png.h>
#include <vector>
#include <stdexcept>
#include <filesystem>
#include <unistd.h>

namespace lab {
// CLI-only capture of the final back buffer, outside sampling, before swap.
// The application owns this context's default read binding and pixel-pack state.
void capture(const std::string& path,tiny2d::Size size) {
    std::vector<png_byte> pixels(std::size_t(size.width)*size.height*4);
    glReadBuffer(GL_BACK);
    glReadPixels(0,0,size.width,size.height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    tiny2d::detail::checkGL("Lab capture");
    png_image image{};
    image.version=PNG_IMAGE_VERSION;
    image.width=size.width; image.height=size.height; image.format=PNG_FORMAT_RGBA;
    if (!png_image_write_to_file(&image,path.c_str(),0,pixels.data(),-size.width*4,nullptr))
        throw std::runtime_error("PNG capture "+path+": "+image.message);
}

void captureNew(const std::string& path,tiny2d::Size size) {
    namespace fs=std::filesystem;
    auto temporary=(fs::path(path).parent_path()/".survivor-capture-XXXXXX").string();
    const int fd=::mkstemp(temporary.data());
    if (fd<0) throw std::system_error(errno,std::generic_category(),"create temporary screenshot");
    struct Cleanup { const char* path; ~Cleanup() { ::unlink(path); } } cleanup{temporary.c_str()};
    if (::close(fd)<0) throw std::system_error(errno,std::generic_category(),"close temporary screenshot");
    capture(temporary,size);
    fs::create_hard_link(temporary,path);
}
}
