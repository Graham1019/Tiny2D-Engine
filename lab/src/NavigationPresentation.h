#pragma once
#include "MapPresentation.h"
#include "tiny2d_game/NavigationSession.h"

namespace lab {
struct NavigationView {
    bool field=false, path=false, bounds=false;
    int selected=0;
};
void prepareNavigationEnemies(Picture&,const tiny2d::TileMap&,
                              const std::vector<tiny2d_game::NavigationEnemy>&,
                              tiny2d::Vector2 halfSize,TileRange);
void prepareNavigationDebug(Picture&,const tiny2d_game::NavigationSession&,TileRange,const NavigationView&);
void prepareNavigationHud(Picture&,const Display&,tiny2d::Size,const tiny2d_game::NavigationSession&);
}
