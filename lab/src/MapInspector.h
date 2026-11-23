#pragma once
#include "Inspector.h"
#include "tiny2d/MapSession.h"

namespace lab {
struct MapEditor {
    std::uint32_t seedInput;
    bool regenerate=false,bounds=false;
    std::optional<tiny2d::TileCoord> selected;
    std::string error;
};
void mapPanel(ToolState&,MapEditor&,Commands&,const tiny2d::MapSession&,const Display&,bool finite,
              const History&,const tiny2d::TimestampQueries*);
}
