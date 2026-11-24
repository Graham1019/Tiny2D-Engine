#pragma once

#include "tiny2d/TileMap.h"

#include <initializer_list>
#include <stdexcept>
#include <string_view>
#include <utility>

inline tiny2d::TileMap mapFixture(std::initializer_list<std::string_view> rows,
                                  tiny2d::TileCoord spawn, tiny2d::TileCoord exit) {
    const auto width = rows.begin()->size();
    std::vector<tiny2d::Tile> cells;
    for (const auto row : rows) {
        if (row.size() != width) throw std::logic_error("inconsistent fixture width");
        for (char ch : row) cells.push_back(ch == '#' ? tiny2d::Tile::Wall : tiny2d::Tile::Floor);
    }
    return {int(width), int(rows.size()), 16, 0, std::move(cells), spawn, exit};
}
