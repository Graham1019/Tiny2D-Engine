#include "MapPresentation.h"
#include "QuadDrawing.h"
#include <algorithm>
#include <cmath>

namespace lab {
namespace {
using tiny2d::Color;
using tiny2d::TileCoord;
using tiny2d::Vector2;
constexpr Color wall{.16f,.26f,.31f,1}, floor{.018f,.033f,.046f,1};
constexpr Color spawn{.09f,2.25f,1.52f,1}, exitInk{4.1f,1.22f,.16f,1};
constexpr Color playerInk{.83f,1.26f,1.54f,1}, dark{.025f,.045f,.06f,1};

void outline(std::vector<Quad>& out,Vector2 p,Vector2 size,float width,Color color) {
    const float left=p.x-size.x/2,top=p.y-size.y/2;
    quad::rect(out,left,top,size.x,width,color);
    quad::rect(out,left,top+size.y-width,size.x,width,color);
    quad::rect(out,left,top,width,size.y,color);
    quad::rect(out,left+size.x-width,top,width,size.y,color);
}
bool includes(TileRange range,TileCoord cell) {
    return cell.x>=range.left && cell.x<range.right && cell.y>=range.top && cell.y<range.bottom;
}
float centerAxis(float target,float world,float view) {
    return world<=view ? world*.5f : std::clamp(target,view*.5f,world-view*.5f);
}
}

TileRange visibleTileRange(const tiny2d::TileMap& map,const tiny2d::Camera2D& camera,tiny2d::Size logical) {
    const auto min=camera.viewportToWorld({0,0});
    const auto max=camera.viewportToWorld({float(logical.width),float(logical.height)});
    const double size=map.tileSize();
    // Clip in floating point first so an off-map camera never overflows an int.
    auto lower=[size](double point,int limit) { return int(std::clamp(std::floor(point/size),0.,double(limit))); };
    auto upper=[size](double point,int limit) { return int(std::clamp(std::ceil(point/size),0.,double(limit))); };
    return {lower(min.x,map.width()),lower(min.y,map.height()),upper(max.x,map.width()),upper(max.y,map.height())};
}

tiny2d::Camera2D mapCamera(const tiny2d::TileMap& map,Vector2 target,tiny2d::Size logical) {
    tiny2d::Camera2D camera(float(logical.width),float(logical.height));
    constexpr float worldHeight=224;
    camera.setZoom(logical.height/worldHeight);
    camera.setPosition(centerAxis(target.x,map.width()*map.tileSize(),logical.width/camera.zoom()),
                       centerAxis(target.y,map.height()*map.tileSize(),worldHeight));
    return camera;
}

Vector2 mapSurveyPosition(std::uint64_t tick) {
    // A tick-indexed triangle wave keeps the survey identical across seed, grid and framebuffer sizes.
    const auto phase=unsigned(tick%480);
    return {504.f+.4f*(phase<=240 ? phase : 480-phase),312.f};
}

TileRange prepareMapTiles(Picture& picture,const tiny2d::TileMap& map,tiny2d::Size logical) {
    const auto range=visibleTileRange(map,picture.worldCamera,logical);
    picture.tiles.clear();
    picture.tiles.reserve(range.count());
    const float t=map.tileSize();
    for (int y=range.top;y<range.bottom;++y) {
        for (int x=range.left;x<range.right;++x) {
            Color ink=map.isWalkable(x,y) ? floor : wall;
            // A two-tone grid keeps individual cells legible without extra geometry.
            if ((x+y)%2) { ink.r*=.90f; ink.g*=.90f; ink.b*=.90f; }
            quad::rect(picture.tiles,x*t,y*t,t,t,ink);
        }
    }
    return range;
}

void prepareMapActors(Picture& picture,const tiny2d::TileMap& map,Vector2 player,Vector2 halfSize,
                      TileRange visible,bool bounds,std::optional<TileCoord> selected) {
    auto& actors=picture.world;
    actors.clear();
    picture.debug.clear();
    const float t=map.tileSize();
    if (includes(visible,map.spawn())) {
        const auto s=map.tileToWorldCenter(map.spawn().x,map.spawn().y);
        outline(actors,s,{t*.89f,t*.89f},t*.075f,spawn);
    }
    if (includes(visible,map.exit())) {
        const auto e=map.tileToWorldCenter(map.exit().x,map.exit().y);
        quad::ring(actors,e,t*.43f,4,0,t*.14f,exitInk);
        quad::rect(actors,e.x-t*.10f,e.y-t*.10f,t*.2f,t*.2f,exitInk);
    }
    // The visible body and the CPU collider have the same rectangular footprint.
    quad::rect(actors,player.x-halfSize.x,player.y-halfSize.y,2*halfSize.x,2*halfSize.y,playerInk);
    quad::rect(actors,player.x-halfSize.x*.42f,player.y-halfSize.y*.42f,halfSize.x*.84f,halfSize.y*.84f,dark);
    const float stroke=std::min(t*.1f,1.6f/picture.worldCamera.zoom());
    if (bounds) outline(picture.debug,player,{2*halfSize.x,2*halfSize.y},stroke,{1,.47f,.24f,1});
    if (selected && includes(visible,*selected))
        outline(picture.debug,map.tileToWorldCenter(selected->x,selected->y),{t,t},stroke,{.96f,.84f,.42f,1});
}

void prepareMapHud(Picture& picture,const Display& display,tiny2d::Size size,std::uint32_t seed,
                   std::uint64_t tick,bool paused) {
    auto& hud=picture.hud;
    hud.clear();
    if (!display.hud) return;
    picture.hudCamera=tiny2d::Camera2D(size.width,size.height);
    picture.hudCamera.setPosition(size.width*.5f,size.height*.5f);
    const std::string text="SEED "+std::to_string(seed)+"   TICK "+std::to_string(tick)+(paused ? "   PAUSED" : "");
    quad::rect(hud,8,8,18+6*text.size(),23,{.02f,.032f,.043f,.96f});
    quad::rect(hud,8,8,2,23,{.32f,.86f,.72f,1});
    quad::lettering(hud,text,17,16,1,{.87f,.93f,.95f,1});
}
}
