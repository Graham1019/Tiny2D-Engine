#include "NavigationPresentation.h"
#include "QuadDrawing.h"
#include <algorithm>
#include <cctype>

namespace lab {
namespace {
using tiny2d::Color;
using tiny2d::TileCoord;
using tiny2d::Vector2;
using tiny2d_game::NavigationSession;

bool visible(Vector2 p,Vector2 half,TileRange range,float tile) {
    return p.x+half.x>range.left*tile && p.x-half.x<range.right*tile &&
           p.y+half.y>range.top*tile && p.y-half.y<range.bottom*tile;
}
void boundary(std::vector<Quad>& quads,Vector2 p,Vector2 half,float stroke,Color ink) {
    quad::rect(quads,p.x-half.x,p.y-half.y,2*half.x,stroke,ink);
    quad::rect(quads,p.x-half.x,p.y+half.y-stroke,2*half.x,stroke,ink);
    quad::rect(quads,p.x-half.x,p.y-half.y,stroke,2*half.y,ink);
    quad::rect(quads,p.x+half.x-stroke,p.y-half.y,stroke,2*half.y,ink);
}
std::string upper(std::string word) {
    for (char& ch:word) ch=char(std::toupper(static_cast<unsigned char>(ch)));
    return word;
}
}

void prepareNavigationEnemies(Picture& picture,const tiny2d::TileMap& map,
                              const std::vector<tiny2d_game::NavigationEnemy>& enemies,
                              Vector2 halfSize,TileRange range) {
    picture.enemies.clear();
    for (const auto& enemy:enemies) {
        if (!visible(enemy.position,halfSize,range,map.tileSize())) continue;
        quad::rect(picture.enemies,enemy.position.x-halfSize.x,enemy.position.y-halfSize.y,
                   2*halfSize.x,2*halfSize.y,{2.4f,.12f,.21f,1});
    }
}

void prepareNavigationDebug(Picture& picture,const NavigationSession& session,TileRange range,
                            const NavigationView& view) {
    const auto& map=session.world().map();
    const auto& nav=session.navigator();
    const auto& field=nav.field();
    const float tile=map.tileSize();
    const auto half=session.enemyHalfSize();
    const float stroke=1.6f/picture.worldCamera.zoom();
    if (view.field) {
        for (int y=range.top;y<range.bottom;++y) for (int x=range.left;x<range.right;++x) {
            if (!map.isWalkable(x,y)) continue;
            const int distance=field.distance({x,y});
            const float f=distance<0 ? 1.f : distance/(distance+16.f);
            Color color=distance<0 ? Color{.93f,.13f,.47f,.9f} : Color{.15f+.42f*f,.87f-.61f*f,.48f+.42f*f,.82f};
            // A strip leaves the actors and the original tile color readable.
            quad::rect(picture.debug,(x+.13f)*tile,(y+.75f)*tile,tile*.74f,tile*.10f,color);
        }
    }
    if (view.bounds) {
        for (const auto& enemy:session.enemies()) if (visible(enemy.position,half,range,tile))
            boundary(picture.debug,enemy.position,half,stroke,{1,.55f,.66f,1});
    }
    if (!view.path || view.selected<0 || std::size_t(view.selected)>=session.enemies().size()) return;
    const auto& enemy=session.enemies()[view.selected];
    auto position=enemy.position;
    auto cell=enemy.waypoint.value_or(map.worldToTile(position));
    const auto center=[&](TileCoord c) { return map.tileToWorldCenter(c.x,c.y); };
    const Color ink={1,.78f,.25f,1};
    if (enemy.waypoint) {
        quad::segment(picture.debug,position,center(cell),stroke*2,ink);
        position=center(cell);
        quad::ring(picture.debug,position,2.2f,4,0,stroke,ink);
    } else {
        quad::segment(picture.debug,position,center(cell),stroke,ink);
        position=center(cell);
    }
    // An independent display budget also bounds pathological/corrupt input.
    constexpr unsigned maxSegments=256;
    for (unsigned i=0;i<maxSegments;++i) {
        const auto next=nav.next(cell);
        if (!next || field.distance(*next)!=field.distance(cell)-1) break;
        quad::segment(picture.debug,position,center(*next),stroke,ink);
        position=center(*next);
        cell=*next;
    }
    boundary(picture.debug,enemy.position,{half.x+1,half.y+1},stroke,ink);
}

void prepareNavigationHud(Picture& picture,const Display& display,tiny2d::Size size,
                          const NavigationSession& session) {
    auto& hud=picture.hud;
    hud.clear();
    if (!display.hud) return;
    picture.hudCamera=tiny2d::Camera2D(size.width,size.height);
    picture.hudCamera.setPosition(size.width*.5f,size.height*.5f);
    const auto& upgrades=session.upgrades();
    const auto& world=session.world();
    const std::string first="HP "+std::to_string(session.hp())+"/"+std::to_string(upgrades.maxHP())+
        "   TIME "+std::to_string((session.remainingTicks()+59)/60)+"S   ENEMIES "+std::to_string(session.enemies().size());
    std::string second="TICK "+std::to_string(world.tick())+"   "+upper(phaseName(session.phase()));
    if (world.paused()) second+="   PAUSED";
    std::string third;
    if (const auto& last=upgrades.lastChoice())
        third=upper(upgradeName(last->id))+"  LV "+std::to_string(upgrades.level(last->id));
    const auto width=float(18+6*std::max({first.size(),second.size(),third.size()}));
    const float height=third.empty()?33.f:45.f;
    quad::rect(hud,8,8,width,height,{.025f,.038f,.052f,.96f});
    quad::rect(hud,8,8,2,height,{.32f,.86f,.72f,1});
    quad::lettering(hud,first,17,15,1,{.93f,.96f,.97f,1});
    quad::lettering(hud,second,17,27,1,{.66f,.85f,.84f,1});
    if (!third.empty()) quad::lettering(hud,third,17,39,1,{.98f,.79f,.42f,1});
}
}
