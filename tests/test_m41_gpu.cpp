#include "tiny2d/BatchRenderer.h"
#include "tiny2d/Window.h"
#include "../engine/src/Renderer/OpenGL.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
using namespace tiny2d;
namespace {
void check(bool condition,const char* message) {
    if(!condition) throw std::runtime_error(message);
}
template<class F> void logicError(F operation) {
    bool caught=false;
    try {operation();} catch(const std::logic_error& error) {
        caught=true;check(std::string(error.what()).size()>0,"missing diagnostic");
    }
    check(caught,"expected logic_error");
}
bool same(const RenderStats& a,const RenderStats& b) {
    return a.submittedSprites==b.submittedSprites && a.drawCalls==b.drawCalls &&
        a.batchFlushes==b.batchFlushes && a.textureBinds==b.textureBinds &&
        a.uploadedVertexBytes==b.uploadedVertexBytes;
}
auto bindings() {
    std::array<GLint,6> result{};
    const GLenum names[]={GL_CURRENT_PROGRAM,GL_VERTEX_ARRAY_BINDING,GL_ARRAY_BUFFER_BINDING,
        GL_ELEMENT_ARRAY_BUFFER_BINDING,GL_ACTIVE_TEXTURE,GL_TEXTURE_BINDING_2D};
    for(unsigned i=0;i<result.size();++i)glGetIntegerv(names[i],&result[i]);
    return result;
}
void movedFrom(BatchRenderer& r,Texture2D& t,Camera2D& c) {
    logicError([&]{r.beginFrame(c);});logicError([&]{r.draw(t,{0,0},{8,8});});
    logicError([&]{r.flush();});logicError([&]{r.endFrame();});
    logicError([&]{r.stats();});logicError([&]{r.textureSlots();});
}
void contexts() {
    Window a(96,96,"M4.1 Context A");
    Texture2D textureA({1,1,{255,0,0,255}});
    Camera2D camera(96,96);camera.setPosition(48,48);
    {
        // Restore A even if a B-context assertion throws, before textureA destruction.
        struct Restore {Window& window;~Restore(){window.makeContextCurrent();}} restore{a};
        Window b(96,96,"M4.1 unshared Context B");
        Texture2D textureB({1,1,{0,255,0,255}});
        Texture2D moved(std::move(textureB));
        BatchRenderer batch(1,1); // Full queue: rejection must not trigger a flush.
        b.clear(0,0,0,0);batch.beginFrame(camera);batch.draw(moved,{0,0},{96,96});
        const auto state=bindings();const auto stats=batch.stats();
        logicError([&]{textureA.requireCurrentContext();});
        logicError([&]{batch.draw(textureA,{0,0},{96,96});});
        logicError([&]{batch.draw(textureB,{0,0},{96,96});});
        check(state==bindings()&&same(stats,batch.stats()),"rejected texture changed state/stats");
        batch.endFrame();check(batch.stats().drawCalls==1&&batch.stats().submittedSprites==1,"queue changed by rejection");
        unsigned char pixel[4];glReadBuffer(GL_BACK);glReadPixels(10,10,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        check(pixel[0]==0&&pixel[1]==255,"rejected texture changed queued pixels");
    }
    const auto state=bindings();textureA.requireCurrentContext();check(state==bindings(),"validation bound texture");
    BatchRenderer batch;batch.beginFrame(camera);batch.draw(textureA,{0,0},{96,96});batch.endFrame();
    unsigned char pixel[4];glReadBuffer(GL_BACK);glReadPixels(10,10,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    check(pixel[0]==255&&pixel[1]==0,"texture A unusable after recovery");
    detail::checkGL("dual context test");
    std::cout<<"dual unshared contexts, moved-from texture and unchanged rejection queue passed\n";
}
void moves(Camera2D& camera,Texture2D& texture) {
    static_assert(!std::is_nothrow_move_assignable_v<BatchRenderer>);
    static_assert(std::is_nothrow_move_constructible_v<BatchRenderer>);
    for(bool targetActive:{false,true})for(bool sourceActive:{false,true}) {
        BatchRenderer target,source;
        if(targetActive){target.beginFrame(camera);target.draw(texture,{0,0},{8,8});}
        if(sourceActive){source.beginFrame(camera);source.draw(texture,{8,8},{8,8});}
        const auto state=bindings();
        const auto targetStats=target.stats(),sourceStats=source.stats();
        if(targetActive||sourceActive) {
            logicError([&]{target=std::move(source);});
            check(state==bindings()&&same(targetStats,target.stats())&&same(sourceStats,source.stats()),"rejected move changed owners/state");
            // Both-active case unwinds in reverse nesting order without rendering the outer pass early.
            if(sourceActive){source.endFrame();check(source.stats().drawCalls==1,"source queue lost");}
            if(targetActive){target.endFrame();check(target.stats().drawCalls==1,"target queue lost");}
            source.beginFrame(camera);source.endFrame();
        } else {
            target=std::move(source);movedFrom(source,texture,camera);
            check(state==bindings(),"inactive assignment changed bindings");
        }
        target.beginFrame(camera);target.draw(texture,{0,0},{8,8});target.endFrame();
    }
    for(bool active:{false,true}) {
        BatchRenderer source;
        if(active){source.beginFrame(camera);source.draw(texture,{0,0},{8,8});}
        const auto state=bindings();BatchRenderer target(std::move(source));
        check(state==bindings(),"move construction changed GL state");movedFrom(source,texture,camera);
        if(active){target.endFrame();check(target.stats().drawCalls==1,"move construction lost queue");}
        target.beginFrame(camera);target.endFrame();
        auto& alias=target;target=std::move(alias);target.beginFrame(camera);target.endFrame();
        source=std::move(target);source.beginFrame(camera);source.endFrame();
    }
    detail::checkGL("move tests");
    std::cout<<"all four assignment states, active/inactive construction and moved-from APIs passed\n";
}
void slotPixels(Window& window,Camera2D& camera) {
    SpriteRenderer immediate;BatchRenderer batch;
    std::vector<Texture2D> textures;
    for(unsigned i=0;i<9;++i)textures.emplace_back(ImageData{1,1,{
        static_cast<unsigned char>(20+i*25),static_cast<unsigned char>(240-i*23),
        static_cast<unsigned char>(40+(i*47)%210),static_cast<unsigned char>(100+i*17)}});
    check(textures.size()>batch.textureSlots(),"scene must overflow slots");
    // Repeated textures on either side of overflow exercise both fresh and reused slots.
    std::vector<unsigned> order={0,1,0,2,3,4,5,6,7,6,8,0,8,1,2,3,4,5,6,7,8,7};
    auto fb=window.framebufferSize();
    auto reset=[&]{
        glBindFramebuffer(GL_FRAMEBUFFER,0);glDrawBuffer(GL_BACK);glReadBuffer(GL_BACK);
        glViewport(0,0,fb.width,fb.height);glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);
        glDisable(GL_CULL_FACE);glDisable(GL_FRAMEBUFFER_SRGB);glDisable(GL_DITHER);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
        window.clear(.07f,.11f,.13f,.2f);
    };
    std::vector<unsigned char> expected(fb.width*fb.height*4),actual(expected.size()),first;
    for(unsigned reverse=0;reverse<2;++reverse) {
        auto draw=[&](auto& renderer){
            for(unsigned index:order)renderer.draw(textures[index],
                {float(10+index*2),float(12+index*3)},{80,78},index*.035f,{1,.8f,.9f,.65f});
        };
        reset();immediate.beginFrame(camera);draw(immediate);
        glReadPixels(0,0,fb.width,fb.height,GL_RGBA,GL_UNSIGNED_BYTE,expected.data());
        reset();batch.beginFrame(camera);draw(batch);batch.endFrame();
        glReadPixels(0,0,fb.width,fb.height,GL_RGBA,GL_UNSIGNED_BYTE,actual.data());
        // Independently model only the expected slot grouping for stats verification.
        unsigned draws=1,binds=0;std::vector<unsigned> slots;
        for(auto id:order)if(std::find(slots.begin(),slots.end(),id)==slots.end()){
            if(slots.size()==batch.textureSlots()){++draws;slots.clear();}
            slots.push_back(id);++binds;
        }
        const auto stats=batch.stats();
        check(draws>1&&stats.drawCalls==draws&&stats.batchFlushes==draws&&stats.textureBinds==binds&&stats.submittedSprites==order.size(),"cross-slot stats mismatch");
        int error=0;for(std::size_t i=0;i<actual.size();++i)error=std::max(error,std::abs(int(actual[i])-int(expected[i])));
        check(error<=2,"cross-slot framebuffer mismatch (>2/255)");
        if(!reverse)first=expected;else check(first!=expected,"scene does not detect transparent order");
        std::cout<<"cross-slot order "<<reverse<<": max RGBA error="<<error<<", draws/flushes="<<draws<<", binds="<<binds<<'\n';
        std::reverse(order.begin(),order.end());
    }
    detail::checkGL("cross-slot pixel test");
}
}
int main(){try{
    contexts();
    Window window(128,128,"M4.1 review regressions");Camera2D camera(128,128);camera.setPosition(64,64);
    Texture2D texture({1,1,{255,255,255,255}});moves(camera,texture);slotPixels(window,camera);
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
