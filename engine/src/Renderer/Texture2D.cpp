#include "tiny2d/Texture2D.h"
#include "OpenGL.h"
#include <stdexcept>
namespace tiny2d {
Texture2D::Texture2D(const ImageData& image,TextureFilter filter)
    : object_(detail::ObjectKind::Texture),width_(image.width),height_(image.height) {
    GLint maxSize=0; glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxSize);
    if(width_<=0||height_<=0||width_>maxSize||height_>maxSize||
       static_cast<std::uint64_t>(width_)*height_*4!=image.pixels.size()||
       (filter!=TextureFilter::Linear&&filter!=TextureFilter::Nearest)) throw std::invalid_argument("invalid RGBA8 texture dimensions, pixels or filter");
    // Restore pixel-store and binding even when upload fails. No PBO uploads.
    struct Restore {
        GLint alignment=0,binding=0,row=0,rows=0,pixels=0,pbo=0;
        Restore(){ glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment); glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding); glGetIntegerv(GL_UNPACK_ROW_LENGTH,&row); glGetIntegerv(GL_UNPACK_SKIP_ROWS,&rows); glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&pixels); glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&pbo); }
        ~Restore(){ glPixelStorei(GL_UNPACK_ALIGNMENT,alignment); glPixelStorei(GL_UNPACK_ROW_LENGTH,row); glPixelStorei(GL_UNPACK_SKIP_ROWS,rows); glPixelStorei(GL_UNPACK_SKIP_PIXELS,pixels); glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo); glBindTexture(GL_TEXTURE_2D,binding); }
    } restore;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1); glPixelStorei(GL_UNPACK_ROW_LENGTH,0); glPixelStorei(GL_UNPACK_SKIP_ROWS,0); glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
    glBindTexture(GL_TEXTURE_2D,object_.id());
    const GLint f=filter==TextureFilter::Linear?GL_LINEAR:GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,f); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,f);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width_,height_,0,GL_RGBA,GL_UNSIGNED_BYTE,image.pixels.data());
    detail::checkGL("RGBA8 texture upload");
}
void Texture2D::bind(unsigned unit) const {
    object_.requireCurrent("Texture2D::bind");
    GLint maxUnits=0; glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,&maxUnits);
    if(unit>=static_cast<unsigned>(maxUnits)) throw std::invalid_argument("invalid texture unit");
    glActiveTexture(GL_TEXTURE0+unit); glBindTexture(GL_TEXTURE_2D,object_.id()); detail::checkGL("texture bind");
}
}
