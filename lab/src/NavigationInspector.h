#pragma once
#include "Inspector.h"
#include "MapInspector.h"
#include "NavigationPresentation.h"

namespace lab {
void navigationPanel(ToolState&,MapEditor&,NavigationView&,Commands&,const tiny2d_game::NavigationSession&,
                     bool finite,const History&,const tiny2d::TimestampQueries*);
std::optional<tiny2d_game::UpgradeChoice> navigationOffer(const tiny2d_game::NavigationSession&,bool finite);
}
