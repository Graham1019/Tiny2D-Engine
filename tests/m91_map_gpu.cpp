#include "M5Test.h"
#include "MapPresentation.h"
#include "Pipeline.h"
#include "OpenGL.h"
#include "tiny2d/MapGenerator.h"
#include <array>
#include <cmath>

using namespace tiny2d;
using namespace lab;

namespace lab {
struct PipelineProbe {
    static GLuint scene(const Pipeline& pipeline) { return pipeline.targets_->scene.colorName(); }
};
}
namespace {
using Pixel=std::array<float,4>;
struct Image {
    Size size;
    std::vector<Pixel> pixels;
    const Pixel& at(int x,int yFromTop) const {
        return pixels.at((size.height-1-yFromTop)*size.width+x);
    }
};
Image readPixels(GLuint texture,Size size) {
    GLint old;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&old);
    GLuint fbo=0;
    if (texture) {
        glGenFramebuffers(1,&fbo);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
        check(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"map readback FBO complete");
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    } else {
        glBindFramebuffer(GL_READ_FRAMEBUFFER,0);
        glReadBuffer(GL_BACK);
    }
    Image image{size,std::vector<Pixel>(size.width*size.height)};
    glReadPixels(0,0,size.width,size.height,GL_RGBA,GL_FLOAT,image.pixels.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER,old);
    glDeleteFramebuffers(1,&fbo);
    detail::checkGL("map readback");
    return image;
}

TileMap fixture() {
    std::vector<Tile> tiles(12*8,Tile::Floor);
    for (int y=0;y<8;++y)
        for (int x=0;x<12;++x)
            if (!x || x==11 || !y || y==7 || x==4) tiles[y*12+x]=Tile::Wall;
    return {12,8,16,0,std::move(tiles),{2,2},{7,5}};
}

void rasterAndCamera(Window& window) {
    const auto map=fixture();
    Pipeline pipe;
    Picture picture;
    Display display;
    display.hud=false;
    display.bloom=false;
    Stats stats;
    const Size logical{128,96};
    picture.worldCamera=Camera2D(logical.width,logical.height);
    picture.worldCamera.setPosition(64,48);
    auto prepare=[&](bool bounds) {
        const auto range=prepareMapTiles(picture,map,logical);
        prepareMapActors(picture,map,{40,40},{5,5},range,bounds,{});
        prepareMapHud(picture,display,logical,0,0,false);
        return range;
    };
    auto draw=[&](Size pixels) {
        check(pipe.resize(pixels),"map target available");
        pipe.draw(picture,display,stats);
        check(stats.complete && stats.submittedTiles==picture.tiles.size(),"tile submission accounts geometry");
        check(stats.tileDrawCalls==(picture.tiles.size()+2047)/2048,"batch draws use fixed capacity");
        check(stats.tileVertexBytes==144*stats.submittedTiles,"tile upload records actual batch bytes");
        return readPixels(PipelineProbe::scene(pipe),pixels);
    };

    const auto visible=prepare(false);
    check(visible.left==0 && visible.top==0 && visible.right==8 && visible.bottom==6 &&
          visible.count()==48 && picture.tiles.size()==48,"exact edge is half-open");
    const auto scene=draw(logical);
    const auto output=readPixels(0,logical);
    check(std::abs(scene.at(72,40)[1]-.26f)<.001,"wall center position and color");
    check(scene.at(56,40)[1]<.04f,"floor center position and color");
    check(scene.at(43,40)[2]>1.5f && scene.at(48,40)[2]<.1f,"player body edge matches world AABB");
    check(scene.at(33,40)[1]>2.f,"spawn marker writes linear HDR");
    const auto initialTexture=PipelineProbe::scene(pipe);

    display.hud=true;
    prepare(true);
    check(draw(logical).pixels==scene.pixels,"HUD and collision outline leave world HDR unchanged");
    check((stats.mask&(1u<<Debug)) && (stats.mask&(1u<<Overlay)) &&
          readPixels(0,logical).pixels!=output.pixels,"HUD and bounds affect the output buffer");
    display.hud=false;
    prepare(false);
    display.bloom=true;
    check(draw(logical).pixels==scene.pixels,"Bloom leaves HDR source unchanged");
    check(readPixels(0,logical).pixels!=output.pixels && stats.passDraws[Extract]==1,"Bloom changes output pixels");
    check(PipelineProbe::scene(pipe)==initialTexture,"map frames reuse framebuffer storage");

    // Use the real window and framebuffer extents, including a macOS logical resize.
    glfwSetWindowSize(window.nativeHandle(),192,128);
    window.pollEvents();
    const auto resized=window.size(),framebuffer=window.framebufferSize();
    check(resized.width==192 && resized.height==128 && framebuffer.width>0 && framebuffer.height>0,
          "resized window and drawable dimensions");
    picture.worldCamera=Camera2D(resized.width,resized.height);
    picture.worldCamera.setPosition(96,64);
    const auto resizedRange=prepareMapTiles(picture,map,resized);
    prepareMapActors(picture,map,{40,40},{5,5},resizedRange,false,{});
    auto resizedImage=draw(framebuffer);
    auto pixelAt=[&](float x,float y)->Pixel {
        return resizedImage.at(int(x*framebuffer.width/resized.width),int(y*framebuffer.height/resized.height));
    };
    check(std::abs(pixelAt(72,40)[1]-.26f)<.001 && pixelAt(43,40)[2]>1.5f,
          "actual framebuffer scale preserves tile and player positions");
    std::cout<<"map_window="<<resized.width<<'x'<<resized.height<<" framebuffer="
             <<framebuffer.width<<'x'<<framebuffer.height<<'\n';
    check(!glIsTexture(initialTexture),"resizing retires previous map target");

    picture.worldCamera.move(16,0);
    const auto moved=prepareMapTiles(picture,map,resized);
    prepareMapActors(picture,map,{40,40},{5,5},moved,false,{});
    resizedImage=draw(framebuffer);
    check(moved.left==1 && moved.right==12 && pixelAt(27,40)[2]>1.5f &&
          std::abs(pixelAt(56,40)[1]-.26f)<.001,"camera translation shifts both actor and wall");

    picture.worldCamera.setPosition(-1000,-1000);
    check(prepareMapTiles(picture,map,resized).count()==0 && picture.tiles.empty(),"out-of-map culling");
    picture.world.clear();
    draw(framebuffer);
    check(stats.submittedTiles==0 && !stats.tileDrawCalls && !stats.tileVertexBytes,"empty tile range skips batch");
}

void batchesAndLifetime(Window& window) {
    const auto map=generateMap(71,MapParameters{128,96}).map;
    Picture picture;
    Display display;
    display.hud=false; display.bloom=false;
    Stats stats;
    GLuint texture=0;
    const Size extent{160,96};
    {
        Pipeline pipe;
        picture.worldCamera=Camera2D(extent.width,extent.height);
        picture.worldCamera.setPosition(1024,768);
        picture.worldCamera.setZoom(.06f);
        const auto whole=prepareMapTiles(picture,map,extent);
        check(whole.count()==map.capacity(),"zoomed-out view contains entire map");
        pipe.resize(extent);
        glEnable(GL_SCISSOR_TEST);
        glScissor(1,2,3,4);
        glViewport(5,6,7,8);
        glActiveTexture(GL_TEXTURE2);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO,GL_ONE);
        pipe.draw(picture,display,stats);
        GLint viewport[4]{},textureUnit=0,dst=0;
        glGetIntegerv(GL_VIEWPORT,viewport);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&textureUnit);
        glGetIntegerv(GL_BLEND_DST_RGB,&dst);
        check(viewport[0]==5 && viewport[1]==6 && viewport[2]==7 && viewport[3]==8 &&
              glIsEnabled(GL_SCISSOR_TEST) && glIsEnabled(GL_BLEND) && dst==GL_ONE && textureUnit==GL_TEXTURE2,
              "map world pass restores external state");
        check(stats.submittedTiles==12288 && stats.tileDrawCalls==6 && stats.passDraws[World]==6 &&
              stats.tileVertexBytes==12288*144,"maximum map crosses five batch boundaries");
        glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND); glActiveTexture(GL_TEXTURE0);
        texture=PipelineProbe::scene(pipe);
        {
            Window foreign(64,64,"M9.1 foreign context");
            rejects([&] { pipe.draw(picture,display,stats); });
            rejects([&] { pipe.resize({48,48}); });
        }
        window.makeContextCurrent();
        check(glIsTexture(texture) && !pipe.resize({0,0}),"suspension retains owner storage");
        pipe.resize(extent); pipe.draw(picture,display,stats);
        check(stats.complete && PipelineProbe::scene(pipe)==texture,"restore same extent and resume on owner");
    }
    check(!glIsTexture(texture),"pipeline destruction releases map target on its context");
}
}
int main() {
    try {
        Window window(180,128,"M9.1 Tilemap GL");
        rasterAndCamera(window);
        batchesAndLifetime(window);
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
