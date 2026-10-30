#include "tiny2d/BatchRenderer.h"
#include "tiny2d/Window.h"
#include "../engine/src/Renderer/OpenGL.h"
#include <iostream>
#include <vector>
#include <limits>
#include <cmath>
using namespace tiny2d;
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
template<class F> void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,"expected rejection");}
int main(){try{
    Window w(128,128,"M4 GPU regression");glReadBuffer(GL_BACK);Camera2D camera(128,128);camera.setPosition(64,64);
    Texture2D white({1,1,{255,255,255,255}});SpriteRenderer immediate;BatchRenderer batch;
    rejects([&]{batch.endFrame();});rejects([&]{batch.flush();});rejects([&]{batch.draw(white,{0,0},{1,1});});
    rejects([]{BatchRenderer invalid(0);});rejects([]{BatchRenderer invalid(std::numeric_limits<std::size_t>::max());});rejects([]{BatchRenderer invalid(1,0);});
    for(unsigned n:{0,1,500,1000,1001,5000}){
        batch.beginFrame(camera);rejects([&]{batch.beginFrame(camera);});rejects([&]{batch.draw(white,{0,0},{0,1});});
        for(unsigned i=0;i<n;++i)batch.draw(white,{0,0},{1,1});batch.endFrame();
        auto s=batch.stats();check(s.submittedSprites==n&&s.drawCalls==(n+999)/1000&&s.batchFlushes==s.drawCalls&&s.textureBinds==s.drawCalls&&s.uploadedVertexBytes==n*4*9*sizeof(float),"capacity stats");
    }
    immediate.beginFrame(camera);for(int i=0;i<500;++i)immediate.draw(white,{0,0},{1,1});check(immediate.drawCalls()==500,"immediate 500 calls");
    std::vector<Texture2D> textures;for(unsigned i=0;i<batch.textureSlots()+1;++i)textures.emplace_back(ImageData{1,1,{static_cast<unsigned char>(i*29),128,255,255}});
    batch.beginFrame(camera);for(auto& t:textures)batch.draw(t,{0,0},{1,1});batch.endFrame();check(batch.stats().drawCalls==2&&batch.stats().textureBinds==textures.size(),"slot overflow");
    batch.beginFrame(camera);for(int i=0;i<100;++i)batch.draw(textures[i%3],{0,0},{1,1});batch.endFrame();check(batch.stats().drawCalls==1&&batch.stats().textureBinds==3,"slot reuse");
    auto fb=w.framebufferSize();std::vector<unsigned char> expected(fb.width*fb.height*4),actual(expected.size());
    camera.setPosition(60,58);camera.setZoom(1.15f);
    for(bool reverse:{false,true}){
        auto draw=[&](auto& r){for(int i=0;i<12;++i){int j=reverse?11-i:i;r.draw(textures[j%3],{float(15+j*3),float(20+j*2)},{60,60},j*.07f,{1,.7f,.9f,.5f});}};
        w.clear(0,0,0,0);immediate.beginFrame(camera);draw(immediate);glReadPixels(0,0,fb.width,fb.height,GL_RGBA,GL_UNSIGNED_BYTE,expected.data());
        w.clear(0,0,0,0);batch.beginFrame(camera);draw(batch);batch.endFrame();glReadPixels(0,0,fb.width,fb.height,GL_RGBA,GL_UNSIGNED_BYTE,actual.data());
        int maxError=0;for(std::size_t i=0;i<actual.size();++i)maxError=std::max(maxError,std::abs(int(actual[i])-int(expected[i])));check(maxError<=2,"batch/immediate pixel mismatch");std::cout<<"pixel max error "<<maxError<<'\n';
    }
    camera.setPosition(64,64);camera.setZoom(1);
    w.clear(0,0,0,0);batch.beginFrame(camera);batch.flush();batch.draw(white,{0,0},{128,128},0,{1,0,0,.5});batch.draw(white,{0,0},{128,128},0,{0,0,1,.5});batch.endFrame();unsigned char p[4];glReadPixels(fb.width/2,fb.height/2,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p);check(std::abs(int(p[0])-64)<=2&&std::abs(int(p[2])-128)<=2&&std::abs(int(p[3])-192)<=2,"RGBA straight alpha");
    glActiveTexture(GL_TEXTURE3);white.bind(3);glPixelStorei(GL_UNPACK_ALIGNMENT,8);glPixelStorei(GL_UNPACK_ROW_LENGTH,7);glPixelStorei(GL_UNPACK_SKIP_ROWS,2);glPixelStorei(GL_UNPACK_SKIP_PIXELS,1);
    GLuint pbo;glGenBuffers(1,&pbo);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo);glBufferData(GL_PIXEL_UNPACK_BUFFER,512,nullptr,GL_STATIC_DRAW);
    {Texture2D t({1,1,{1,2,3,4}});GLint value=0;for(auto pair:std::vector<std::pair<GLenum,GLint>>{{GL_ACTIVE_TEXTURE,GL_TEXTURE3},{GL_TEXTURE_BINDING_2D,GLint(white.nativeHandle())},{GL_UNPACK_ALIGNMENT,8},{GL_UNPACK_ROW_LENGTH,7},{GL_UNPACK_SKIP_ROWS,2},{GL_UNPACK_SKIP_PIXELS,1},{GL_PIXEL_UNPACK_BUFFER_BINDING,GLint(pbo)}}){glGetIntegerv(pair.first,&value);check(value==pair.second,"texture upload state restoration");}}
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glDeleteBuffers(1,&pbo);glPixelStorei(GL_UNPACK_ALIGNMENT,4);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
    glUseProgram(0);glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);glDisable(GL_BLEND);glBlendFuncSeparate(GL_ONE,GL_ZERO,GL_ZERO,GL_ONE);
    batch.beginFrame(camera);batch.draw(white,{0,0},{1,1});batch.endFrame();GLint v;glGetIntegerv(GL_ACTIVE_TEXTURE,&v);check(v==GL_TEXTURE3&&!glIsEnabled(GL_BLEND),"batch state restoration");glGetIntegerv(GL_CURRENT_PROGRAM,&v);check(v==0,"program restore");
    GLuint va=0,vb=0,eb=0,program=0;{BatchRenderer source;source.beginFrame(camera);source.draw(white,{0,0},{1,1});source.flush();glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&v);va=v;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&v);vb=v;glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&v);eb=v;glGetIntegerv(GL_CURRENT_PROGRAM,&v);program=v;BatchRenderer moved(std::move(source));rejects([&]{source.beginFrame(camera);});moved.endFrame();BatchRenderer target;target=std::move(moved);target.beginFrame(camera);target.endFrame();}
    check(!glIsVertexArray(va)&&!glIsBuffer(vb)&&!glIsBuffer(eb)&&!glIsProgram(program),"batch lifetime leak");check(glGetError()==GL_NO_ERROR,"unexpected GL error");
    std::cout<<"M4 capacity, slots, lifecycle, RGBA pixels, state and resource tests passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
