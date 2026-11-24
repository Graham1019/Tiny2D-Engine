#include "M5Test.h"
#include "tiny2d/TileCollision.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

using namespace tiny2d;

namespace {

bool same(Vector2 a, Vector2 b) {
    return a.x == b.x && a.y == b.y;
}

TileMap fixture(bool corridor = false, float size = 16.f) {
    constexpr int w = 11, h = 9;
    std::vector<Tile> cells(w*h, corridor ? Tile::Wall : Tile::Floor);
    if (corridor) {
        for (int x = 1; x < w-1; ++x) cells[2*w+x] = Tile::Floor;
    } else {
        cells[4*w+5] = Tile::Wall;
        cells[6*w+7] = Tile::Wall;
    }
    return {w, h, size, 42, std::move(cells), {1,2}, {9,2}};
}

// Independent reference: enumerate every solid rectangle at each axis, using
// the closest admissible endpoint among those with overlapping lateral spans.
Vector2 bruteEndpoint(const TileMap& map, Vector2 p, Vector2 half, Vector2 delta) {
    for (int axis = 0; axis < 2; ++axis) {
        float& start = axis == 0 ? p.x : p.y;
        const float side = axis == 0 ? p.y : p.x;
        const float radius = axis == 0 ? half.x : half.y;
        const float lateral = axis == 0 ? half.y : half.x;
        const float d = axis == 0 ? delta.x : delta.y;
        const int count = axis == 0 ? map.width() : map.height();
        float end = std::clamp(start + d, radius, count*map.tileSize() - radius);
        for (int y = 0; y < map.height(); ++y) {
            for (int x = 0; x < map.width(); ++x) {
                if (map.isWalkable(x,y)) continue;
                const float near = (axis == 0 ? x : y)*map.tileSize();
                const float across = (axis == 0 ? y : x)*map.tileSize();
                if (side+lateral <= across || side-lateral >= across+map.tileSize()) continue;
                if (d > 0 && start+radius <= near) end = std::min(end, near-radius);
                if (d < 0 && start-radius >= near+map.tileSize())
                    end = std::max(end, near+map.tileSize()+radius);
            }
        }
        start = end;
    }
    return p;
}

void wallsAndCorners() {
    const auto map = fixture();
    const Vector2 half{5,5};
    auto move = [&](Vector2 p, Vector2 d) { return moveAgainstTiles(map, p, half, d); };
    auto nose = move({40,72}, {100,0});
    check(same(nose.position, {75,72}) && nose.hitX && !nose.hitY, "face stop");
    auto flush = move({72,72}, {3,0});
    check(flush.position.x == 75 && !flush.hitX, "exact contact is legal");
    auto away = move(nose.position, {-2,0});
    check(same(away.position,{73,72}) && !away.hitX, "reverse while flush");
    auto parallel = move(nose.position, {0,7});
    check(same(parallel.position,{75,79}) && !parallel.hitX && !parallel.hitY, "parallel slide");
    auto diagonal = move({72,72}, {16,8});
    check(same(diagonal.position,{75,80}) && diagonal.hitX && !diagonal.hitY, "diagonal slide");
    auto corner = move({72,56}, {16,16});
    check(same(corner.position,{88,59}) && !corner.hitX && corner.hitY, "X precedes Y at corner");
    check(same(move({75,59}, {16,0}).position,{91,59}), "corner tangent");
    check(!canOccupyTiles(map,{std::nextafter(75.f,76.f),72},half), "one ULP penetration");
    auto stop = move(nose.position, {0,0});
    check(same(stop.position,nose.position) && !stop.hitX && !stop.hitY, "zero movement");

    const float large = std::numeric_limits<float>::max();
    auto highSpeed = move({8,72}, {large,0});
    check(same(highSpeed.position,{75,72}) && highSpeed.hitX, "positive swept wall");
    auto fromRight = move({168,72}, {-large,0});
    check(same(fromRight.position,{101,72}) && fromRight.hitX, "negative swept wall");
    check(highSpeed.tilesQueried < 10 && fromRight.tilesQueried < 10, "sweep queries local strips");
    auto upper = move({40,40}, {large,large});
    auto lower = move({40,40}, {-large,-large});
    check(same(upper.position,{171,139}) && upper.hitX && upper.hitY, "upper outside barriers");
    check(same(lower.position,{5,5}) && lower.hitX && lower.hitY, "lower outside barriers");
    const auto repeated = move({40,40}, {large,large});
    check(same(repeated.position,upper.position) && repeated.hitX == upper.hitX &&
          repeated.hitY == upper.hitY && repeated.tilesQueried == upper.tilesQueried,
          "repeat every result field");
    const auto corridor = fixture(true);
    auto passage = moveAgainstTiles(corridor,{24,40},half,{120,0});
    check(same(passage.position,{144,40}) && !passage.hitX, "one-cell corridor");
    auto rails = moveAgainstTiles(corridor,{24,40},half,{120,10});
    check(same(rails.position,{144,43}) && rails.hitY, "corridor side clearance");
}

void invalidAndFractional() {
    const auto map = fixture();
    const Vector2 half{5,5}, start{24,40};
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    check(canOccupyTiles(map, start, half), "spawn clear");
    for (Vector2 p : {Vector2{-1,40}, {1,40}, {88,72}, {nan,40}, {24,-inf}}) {
        check(!canOccupyTiles(map,p,half), "invalid initial box");
        rejects([&]{ moveAgainstTiles(map,p,half,{}); });
    }
    for (Vector2 h : {Vector2{0,5}, {-1,5}, {5,inf}, {nan,5}, {1000,1000}})
        rejects([&]{ moveAgainstTiles(map,start,h,{}); });
    for (Vector2 d : {Vector2{nan,0}, {0,inf}, {-inf,1}})
        rejects([&]{ moveAgainstTiles(map,start,half,d); });

    const auto fraction = fixture(false, 0.1f);
    const Vector2 small{0.0333f,0.02f};
    const auto touch = moveAgainstTiles(fraction, {0.35f,0.45f}, small, {0.6f,0});
    check(touch.hitX && canOccupyTiles(fraction,touch.position,small), "round contact toward free space");
    check(!canOccupyTiles(fraction,{std::nextafter(touch.position.x,1.f),touch.position.y},small),
          "nearest legal contact float");
    const float size = std::ldexp(1.f, 100);
    const auto huge = fixture(false,size);
    auto largeScale = moveAgainstTiles(huge,{5.f,4.5f*size},half,{5.f*size,0.f});
    check(largeScale.hitX && canOccupyTiles(huge,largeScale.position,half),
          "tiny box remains outside huge-coordinate wall");

    // The exact requested center is 5 * size, but its float representation
    // rounds down. The tiny box really hits the wall during this displacement.
    const float oddSize = std::nextafter(1.f,2.f), tiny = std::ldexp(1.f,-65);
    const auto odd = fixture(false,oddSize);
    auto subUlp = moveAgainstTiles(odd,{4*oddSize,4.5f*oddSize},{tiny,tiny},{oddSize,0});
    check(subUlp.hitX && canOccupyTiles(odd,subUlp.position,{tiny,tiny}),
          "unrounded swept endpoint records sub-ULP contact");
}

void compareToReference(float size) {
    const auto map = fixture(false, size);
    const float scale = size / 16.f;
    std::uint32_t random = 0xace1u;
    auto integer = [&](int limit) {
        random = 1664525u*random + 1013904223u;
        return static_cast<int>((random >> 8) % static_cast<unsigned>(limit));
    };
    int cases = 0;
    for (int trial = 0; trial < 2500; ++trial) {
        Vector2 center{integer(352)*0.5f*scale, integer(288)*0.5f*scale};
        Vector2 half{(1.f+integer(28)*0.5f)*scale, (1.f+integer(28)*0.5f)*scale};
        Vector2 delta{(integer(1001)*0.5f-250.f)*scale, (integer(1001)*0.5f-250.f)*scale};
        if (!canOccupyTiles(map,center,half)) continue;
        const auto move = moveAgainstTiles(map,center,half,delta);
        check(same(move.position,bruteEndpoint(map,center,half,delta)), "full enumeration reference");
        check(canOccupyTiles(map,move.position,half), "swept endpoint legal");
        ++cases;
    }
    check(cases > 1000, "reference sample count");
    std::cout << cases << " full-enumeration sweep comparisons at tile size " << size << '\n';
}

} // namespace

int main() {
    try {
        wallsAndCorners();
        invalidAndFractional();
        for (float size : {16.f, 0.125f, std::ldexp(1.f,-70)}) compareToReference(size);
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
