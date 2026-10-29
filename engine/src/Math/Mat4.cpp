#include "tiny2d/Mat4.h"
#include <cmath>
#include <stdexcept>
namespace tiny2d {
Mat4 Mat4::identity() { Mat4 m; m.values[0]=m.values[5]=m.values[10]=m.values[15]=1; return m; }
Mat4 Mat4::translation(float x,float y) { auto m=identity(); m.values[12]=x; m.values[13]=y; return m; }
Mat4 Mat4::scale(float x,float y) { auto m=identity(); m.values[0]=x; m.values[5]=y; return m; }
Mat4 Mat4::rotationZ(float a) { auto m=identity(); m.values[0]=m.values[5]=std::cos(a); m.values[1]=std::sin(a); m.values[4]=-std::sin(a); return m; }
Mat4 Mat4::orthographic(float l,float r,float t,float b) {
    if (!std::isfinite(l)||!std::isfinite(r)||!std::isfinite(t)||!std::isfinite(b)||r<=l||b<=t||!std::isfinite(r-l)||!std::isfinite(b-t)) throw std::invalid_argument("invalid orthographic bounds");
    auto m=identity(); m.values[0]=2/(r-l); m.values[5]=-2/(b-t); m.values[10]=-1;
    m.values[12]=-1-l*m.values[0]; m.values[13]=1-t*m.values[5]; return m;
}
Mat4 Mat4::operator*(const Mat4& rhs) const {
    Mat4 m;
    for(int c=0;c<4;++c) for(int row=0;row<4;++row) for(int k=0;k<4;++k)
        m.values[c*4+row]+=values[k*4+row]*rhs.values[c*4+k];
    return m;
}
}
