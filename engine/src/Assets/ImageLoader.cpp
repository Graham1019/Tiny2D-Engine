#include "tiny2d/ImageLoader.h"
#include <png.h>
#include <limits>
#include <stdexcept>
namespace tiny2d {
ImageData ImageLoader::loadPNG(const std::filesystem::path& path) {
    struct Decoder { png_image image{}; Decoder(){ image.version=PNG_IMAGE_VERSION; } ~Decoder(){ png_image_free(&image); } } decoder;
    auto& im=decoder.image;
    if(!png_image_begin_read_from_file(&im,path.string().c_str())) throw std::runtime_error("PNG open/decode failed: "+path.string()+": "+im.message);
    // Explicit allocation budget also rejects hostile dimensions before allocation.
    if(!im.width||!im.height||im.width>16384||im.height>16384||
       static_cast<std::uint64_t>(im.width)*im.height>64*1024*1024||
       static_cast<std::uint64_t>(im.width)*im.height>std::numeric_limits<std::size_t>::max()/4)
        throw std::runtime_error("PNG dimensions exceed supported allocation budget: "+path.string());
    im.format=PNG_FORMAT_RGBA;
    ImageData result{static_cast<int>(im.width),static_cast<int>(im.height),{}};
    result.pixels.resize(static_cast<std::size_t>(im.width)*im.height*4);
    if(!png_image_finish_read(&im,nullptr,result.pixels.data(),0,nullptr)) throw std::runtime_error("PNG decode failed: "+path.string()+": "+im.message);
    return result;
}
}
