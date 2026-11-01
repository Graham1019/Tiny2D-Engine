#include "tiny2d/Camera2D.h"
#include "tiny2d/ImageLoader.h"
#include "ShaderAssets.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace tiny2d;
void check(bool ok) { if(!ok) throw std::runtime_error("M3 CPU assertion failed"); }
void near(float a,float b) { check(std::abs(a-b)<0.0001f); }
void point(const Mat4& m,float x,float y,float ex,float ey) {
    near(m.values[0]*x+m.values[4]*y+m.values[12],ex);
    near(m.values[1]*x+m.values[5]*y+m.values[13],ey);
}
template<class F> void rejects(F f) { bool caught=false; try { f(); } catch(const std::exception&) {caught=true;} check(caught); }
int main() { try {
    point(Mat4::identity(),2,3,2,3);
    point(Mat4::translation(4,5)*Mat4::scale(2,3),1,2,6,11);
    point(Mat4::scale(2,3)*Mat4::translation(4,5),1,2,10,21);
    point(Mat4::rotationZ(3.14159265f/2),1,0,0,1);
    point(Mat4::orthographic(0,800,0,600),0,0,-1,1);
    point(Mat4::orthographic(0,800,0,600),800,600,1,-1);
    Camera2D c(800,600); c.setPosition(400,300);
    point(c.viewProjectionMatrix(),400,300,0,0);
    c.setZoom(2); point(c.viewProjectionMatrix(),600,450,1,-1);
    c.move(20,30); point(c.viewProjectionMatrix(),420,330,0,0);
    c.setViewportSize(400,200); point(c.viewProjectionMatrix(),520,380,1,-1);
    rejects([&]{c.setZoom(0);}); rejects([&]{c.setZoom(-1);});
    rejects([&]{c.setZoom(std::numeric_limits<float>::infinity());});
    rejects([&]{c.setViewportSize(0,1);}); rejects([&]{c.setViewportSize(1,-1);});
    rejects([&]{c.setPosition(NAN,0);}); rejects([]{Mat4::orthographic(0,0,0,1);});
    auto im=ImageLoader::loadPNG(assetPath("textures/sprite.png"));
    check(im.width==32&&im.height==32&&im.pixels.size()==4096);
    check(im.pixels[3]==0&&im.pixels[(8*32+8)*4+3]==255&&im.pixels[(8*32+24)*4+3]==128);
    check(im.pixels[(8*32+8)*4+1]==80&&im.pixels[(24*32+8)*4+1]==220);
    rejects([]{ImageLoader::loadPNG(assetPath("missing.png"));});
    rejects([]{ImageLoader::loadPNG(assetPath("shaders/triangle.vert"));});
    auto path=assetPath("textures/sprite.png");
    std::ifstream f(path,std::ios::binary); std::string bytes{std::istreambuf_iterator<char>(f),{}};
    auto bad=std::filesystem::temp_directory_path()/"tiny2d-m3-truncated.png";
    { std::ofstream out(bad,std::ios::binary); out.write(bytes.data(),bytes.size()/2); }
    rejects([&]{ImageLoader::loadPNG(bad);}); std::filesystem::remove(bad);
    std::cout<<"M3 matrix, camera and PNG tests passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;} }
