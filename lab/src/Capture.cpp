#include "Capture.h"
#include "OpenGL.h"
#include <png.h>
#include <vector>
#include <stdexcept>

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
}
