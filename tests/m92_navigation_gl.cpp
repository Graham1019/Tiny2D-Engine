#include "M5Test.h"
#include "NavigationInspector.h"
#include "NavigationPresentation.h"
#include "OpenGL.h"
#include "Pipeline.h"
#include "imgui_impl_glfw.h"
#include "imgui_internal.h"
#include <array>
#include <cmath>

using namespace lab;
using namespace tiny2d;
using namespace tiny2d_game;

namespace lab {
struct PipelineProbe {
    static GLuint scene(const Pipeline& pipe) { return pipe.targets_->scene.colorName(); }
};
}
namespace {
using Pixel=std::array<float,4>;
struct Image {
    Size size;
    std::vector<Pixel> data;
    const Pixel& at(Vector2 world) const {
        // Both camera setups in this test fill the same 256 x 192 world rectangle.
        int x=int(world.x*size.width/256), y=int(world.y*size.height/192);
        return data.at((size.height-1-y)*size.width+x);
    }
};
Image readImage(GLuint texture,Size size) {
    GLint saved;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&saved);
    GLuint read=0;
    if (texture) {
        glGenFramebuffers(1,&read);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,read);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
        check(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"readback source complete");
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    } else {
        glBindFramebuffer(GL_READ_FRAMEBUFFER,0);
        glReadBuffer(GL_BACK);
    }
    Image out{size,std::vector<Pixel>(size.width*size.height)};
    glReadPixels(0,0,size.width,size.height,GL_RGBA,GL_FLOAT,out.data.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER,saved);
    glDeleteFramebuffers(1,&read);
    detail::checkGL("navigation readback");
    return out;
}

void renderLayers(Window& window) {
    NavigationSession session(71,MapParameters{16,12},4);
    const auto& map=session.world().map();
    const auto actor=session.enemies().front().position;
    const auto player=session.world().playerPosition();
    const auto builds=session.navigator().stats().fieldRebuildCount;
    GLuint texture=0;
    {
        Pipeline pipe;
        Picture pic;
        Display display;
        display.hud=false;
        Stats stats;
        for (auto width : {256,384}) {
            glfwSetWindowSize(window.nativeHandle(),width,width*3/4);
            window.pollEvents();
            const auto logical=window.size(), pixels=window.framebufferSize();
            check(logical.width==width && pixels.width>=logical.width,"real framebuffer resize");
            pipe.resize(pixels);
            if (texture) check(!glIsTexture(texture),"resize releases previous attachment");
            texture=PipelineProbe::scene(pipe);
            pic.worldCamera=Camera2D(logical.width,logical.height);
            pic.worldCamera.setZoom(logical.width/256.f);
            pic.worldCamera.setPosition(128,96);
            const auto range=prepareMapTiles(pic,map,logical);
            prepareMapActors(pic,map,player,{5,5},range,false,{});
            prepareNavigationEnemies(pic,map,session.enemies(),{5,5},range);
            glEnable(GL_SCISSOR_TEST);
            glScissor(1,2,3,4);
            glViewport(5,6,7,8);
            pipe.draw(pic,display,stats);
            GLint viewport[4];
            glGetIntegerv(GL_VIEWPORT,viewport);
            check(glIsEnabled(GL_SCISSOR_TEST) && viewport[0]==5 && viewport[3]==8,"restore caller GL state");
            glDisable(GL_SCISSOR_TEST);
            const auto hdr=readImage(texture,pixels), output=readImage(0,pixels);
            check(stats.enemyDrawCalls==1 && stats.enemyVertexBytes==4*144 && stats.submittedEnemies==4,
                  "one real draw for four enemies");
            check(hdr.at(actor)[0]>2 && hdr.at(actor)[1]<.2f && hdr.at({player.x+3,player.y})[2]>1.5f,
                  "spawned enemy and player occupy distinct expected HDR pixels");
            check(std::abs(hdr.at({8,8})[1]-.26f)<.002,"map wall at world origin");
            for (bool path : {false,true}) {
                pic.debug.clear();
                prepareNavigationDebug(pic,session,range,NavigationView{!path,path,false,0});
                pipe.draw(pic,display,stats);
                const auto overlay=readImage(0,pixels);
                check(readImage(texture,pixels).data==hdr.data,"debug geometry stays outside HDR and bright extraction");
                if (path) {
                    auto next=session.navigator().next(map.worldToTile(actor));
                    check(bool(next),"selected enemy has a legal first hop");
                    const auto nextCenter=map.tileToWorldCenter(next->x,next->y);
                    Vector2 middle{(actor.x+nextCenter.x)*.5f,(actor.y+nextCenter.y)*.5f};
                    check(overlay.at(middle)!=output.at(middle),"planned segment at expected midpoint");
                } else {
                    Vector2 strip{player.x,player.y+4.7f};
                    check(overlay.at(strip)!=output.at(strip),"field annotation at target tile");
                }
            }
            check(session.navigator().stats().fieldRebuildCount==builds,"drawing cannot rebuild navigation");
            std::cout<<"navigation_raster logical="<<logical.width<<'x'<<logical.height<<" pixels="
                     <<pixels.width<<'x'<<pixels.height<<'\n';
        }
        {
            Window other(64,64,"Navigation foreign context");
            rejects([&]{pipe.draw(pic,display,stats);});
            rejects([&]{pipe.resize({32,32});});
        }
        window.makeContextCurrent();
        check(glIsTexture(texture),"foreign context leaves owner resource live");
        pipe.draw(pic,display,stats);
        check(stats.complete,"resumed owner draws enemies");
    }
    check(!glIsTexture(texture),"owner releases final texture");
}

// Drive the real offer widget through the GLFW backend, with the Inspector hidden.
void offerInput(Window& window) {
    glfwSetWindowSize(window.nativeHandle(),800,450);
    window.pollEvents();
    NavigationSession session(71,{},0);
    for (int i=0;i<600;++i) session.step();
    check(session.phase()==NavigationPhase::AwaitingUpgrade,"first offer at 600");
    const auto offer=*session.upgrades().offer();
    const auto pos=session.world().playerPosition();
    Inspector gui(window,false);
    ToolState tools{true,false,false};
    Pipeline pipe;
    const auto pixels=window.framebufferSize();
    pipe.resize(pixels);
    Display display;
    display.hud=display.bloom=false;
    Picture picture;
    Stats stats;
    pipe.draw(picture,display,stats);
    const auto blank=readImage(0,pixels);
    auto* w=window.nativeHandle();
    ImGui_ImplGlfw_CursorEnterCallback(w,GLFW_TRUE);
    auto frame=[&](bool focused=true) {
        gui.buildPanel(tools,focused,[](Commands&){},false,true);
        auto selected=navigationOffer(session,false);
        FrameInfo info;
        info.display=display;
        gui.finish(tools,pipe,info,picture,{},true);
        DrawTools draw;
        draw.gui=[&](Stats& s){gui.submit(s);};
        pipe.draw(picture,display,stats,draw);
        return selected;
    };
    frame(); frame();
    ImGui_ImplGlfw_KeyCallback(w,GLFW_KEY_F2,0,GLFW_PRESS,0);
    frame();
    ImGui_ImplGlfw_KeyCallback(w,GLFW_KEY_F2,0,GLFW_RELEASE,0);
    frame();
    check(!tools.visible && ImGui::FindWindowByName("Choose an upgrade"),"offer survives F2 hiding Inspector");
    const auto shown=readImage(0,pixels);
    check((stats.mask&(1u<<Gui)) && stats.guiVertices>0 && shown.at({128,96})!=blank.at({128,96})
          && shown.at({1,1})==blank.at({1,1}),"hidden-Inspector offer rasterized over world output");
    session.setPaused(true);
    session.step({1,0});
    session.fixedUpdate({-1,0});
    check(session.world().tick()==600 && session.world().playerPosition().x==pos.x,"offer blocks single step");
    auto aim=[&] {
        auto* dialog=ImGui::FindWindowByName("Choose an upgrade");
        const auto buttonY=dialog->Pos.y+dialog->TitleBarHeight+ImGui::GetStyle().WindowPadding.y+
                           ImGui::GetTextLineHeightWithSpacing()+ImGui::GetFrameHeight()*.5f;
        ImGui_ImplGlfw_CursorPosCallback(w,dialog->Pos.x+ImGui::GetStyle().WindowPadding.x+35,buttonY);
        frame();
    };
    aim();
    ImGui_ImplGlfw_MouseButtonCallback(w,0,GLFW_PRESS,0);
    check(!frame(),"button press alone cannot choose");
    const auto callback=glfwSetWindowFocusCallback(w,nullptr);
    glfwSetWindowFocusCallback(w,callback);
    callback(w,GLFW_FALSE);
    check(!frame(false),"focus loss cancels the pressed offer button");
    callback(w,GLFW_TRUE);
    ImGui_ImplGlfw_MouseButtonCallback(w,0,GLFW_RELEASE,0);
    check(!frame() && !frame(),"stale release after focus gain cannot choose");
    aim();
    ImGui_ImplGlfw_MouseButtonCallback(w,0,GLFW_PRESS,0);
    frame();
    ImGui_ImplGlfw_MouseButtonCallback(w,0,GLFW_RELEASE,0);
    auto selected=frame();
    check(selected && selected->epoch==offer.epoch && selected->serial==offer.serial,
          "fresh click chooses a displayed offer after focus restoration");
    check(session.choose(selected->epoch,selected->serial,selected->id),"choice applied");
    check(!session.choose(selected->epoch,selected->serial,selected->id),"duplicate choice rejected");
    check(session.phase()==NavigationPhase::Playing && session.world().paused() && session.world().tick()==600,
          "choice preserves manual pause and does not advance tick");
}
}
int main() {
    try {
        Window window(256,192,"M9.2 Navigation GL");
        renderLayers(window);
        offerInput(window);
        std::cout<<"navigation HDR, overlays, resource lifetime and hidden-Inspector offer interaction passed\n";
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
