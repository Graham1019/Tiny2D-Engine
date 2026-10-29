#include "tiny2d/SpriteRenderer.h"
#include "tiny2d/Window.h"
#include "../engine/src/Renderer/OpenGL.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <thread>
using namespace tiny2d;
void check(bool ok,const char* msg) {if(!ok) throw std::runtime_error(msg);}
std::array<unsigned char,4> pixel(Window& w,float x,float y) {
    auto fb=w.framebufferSize(); auto sz=w.size(); std::array<unsigned char,4> p{};
    glReadPixels(int(x*fb.width/sz.width),int((sz.height-y)*fb.height/sz.height),1,1,GL_RGBA,GL_UNSIGNED_BYTE,p.data()); return p;
}
int main(){ try {
    Window w(200,200,"M3 GPU tests"); glReadBuffer(GL_BACK);
    GLuint id=0;
    {
        ImageData image{2,2,{255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,0}};
        glPixelStorei(GL_UNPACK_ALIGNMENT,8);
        Texture2D original(image,TextureFilter::Nearest); id=original.nativeHandle();
        GLint alignment=0; glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment); check(alignment==8,"unpack alignment not restored");
        Texture2D moved(std::move(original)); check(original.nativeHandle()==0,"texture move source owns handle");
        Texture2D texture(image); auto replaced=texture.nativeHandle(); texture=std::move(moved);
        check(!glIsTexture(replaced)&&moved.nativeHandle()==0,"texture move assignment leak");
        bool rejected=false;
        std::thread worker([&]{try {texture.bind();} catch(const std::logic_error&) {rejected=true;}}); worker.join();
        check(rejected,"texture worker thread accepted");
        glfwMakeContextCurrent(nullptr); rejected=false;
        try {texture.bind();} catch(const std::logic_error&) {rejected=true;}
        w.makeContextCurrent(); check(rejected,"texture accepted missing context");
        rejected=false; try {original.bind();} catch(const std::logic_error&) {rejected=true;}
        check(rejected,"moved-from texture accepted");
        texture.bind(1); std::array<unsigned char,16> data{}; glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,data.data());
        check(std::equal(data.begin(),data.end(),image.pixels.begin()),"texture upload/readback differs");
        SpriteRenderer renderer; Camera2D camera(200,200); camera.setPosition(100,100);
        w.clear(0,0,0); renderer.beginFrame(camera); renderer.draw(texture,{20,20},{80,80});
        check(pixel(w,30,30)[0]>250,"top-left red UV"); check(pixel(w,90,30)[1]>250,"top-right green UV");
        check(pixel(w,30,90)[2]>250,"bottom-left blue UV"); check(pixel(w,90,90)[0]==0,"transparent texel");
        w.clear(0,0,0); renderer.draw(texture,{20,20},{80,80},3.14159265f/2);
        check(pixel(w,90,30)[0]>250,"clockwise center rotation");
        Texture2D white({1,1,{255,255,255,255}});
        w.clear(0,0,0); renderer.beginFrame(camera);
        renderer.draw(white,{40,40},{80,80},0,{1,0,0,0.5}); renderer.draw(white,{40,40},{80,80},0,{0,0,1,0.5});
        auto p=pixel(w,60,60); check(p[0]>=62&&p[0]<=66&&p[2]>=126&&p[2]<=130,"straight alpha/order");
        check(renderer.drawCalls()==2,"draw counter");
        w.clear(0,0,0); renderer.beginFrame(camera);
        renderer.draw(white,{40,40},{80,80},0,{0,0,1,0.5}); renderer.draw(white,{40,40},{80,80},0,{1,0,0,0.5});
        p=pixel(w,60,60); check(p[0]>125&&p[2]<66,"reverse order");
        glfwSetWindowSize(glfwGetCurrentContext(),300,240); w.pollEvents(); auto size=w.size(); camera.setViewportSize(size.width,size.height);
        camera.setPosition(100,100); camera.setZoom(2); w.clear(0,0,0); renderer.beginFrame(camera);
        renderer.draw(white,{95,95},{10,10}); check(pixel(w,size.width/2,size.height/2)[0]>250,"resize camera center");
        check(pixel(w,10,10)[0]==0,"resize background");
        GLint vp[4];glGetIntegerv(GL_VIEWPORT,vp);check(vp[2]==w.framebufferSize().width,"framebuffer viewport");
        check(glGetError()==GL_NO_ERROR,"GL error");
    }
    check(!glIsTexture(id),"texture destruction leak");
    std::cout<<"M3 GPU texture, uniforms, UV, transform, blend, order, resize and lifetime passed\n";
} catch(const std::exception& e){ std::cerr<<e.what()<<'\n';return 1;} }
