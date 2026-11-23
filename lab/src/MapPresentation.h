#pragma once
#include "Presentation.h"
#include "tiny2d/TileMap.h"

namespace lab {
// Half-open tile intervals. Empty ranges are valid when looking outside the map.
struct TileRange {
    int left, top, right, bottom;
    unsigned count() const { return unsigned((right-left)*(bottom-top)); }
};
TileRange visibleTileRange(const tiny2d::TileMap&,const tiny2d::Camera2D&,tiny2d::Size logical);
tiny2d::Camera2D mapCamera(const tiny2d::TileMap&,tiny2d::Vector2 target,tiny2d::Size logical);
tiny2d::Vector2 mapSurveyPosition(std::uint64_t tick);
TileRange prepareMapTiles(Picture&,const tiny2d::TileMap&,tiny2d::Size logical);
void prepareMapActors(Picture&,const tiny2d::TileMap&,tiny2d::Vector2 player,tiny2d::Vector2 halfSize,
                      TileRange visible,bool bounds,std::optional<tiny2d::TileCoord> selected);
void prepareMapHud(Picture&,const Display&,tiny2d::Size logical,std::uint32_t seed,std::uint64_t tick,bool paused);
}
