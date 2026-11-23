#include "tiny2d/TileCollision.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tiny2d {
namespace {

bool finite(Vector2 value) {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

struct TileSpan {
    int begin, end; // half-open indices
};

template<class Predicate>
int firstMatching(int count, Predicate predicate) {
    int first = 0;
    while (first < count) {
        const int middle = first + (count - first) / 2;
        if (predicate(middle)) count = middle;
        else first = middle + 1;
    }
    return first;
}

bool crosses(double center, double half, double plane, int sign) {
    // Subtract nearby operands to retain small separations. A tiny box on a
    // large grid line can straddle it even when center + half rounds to center.
    if (sign > 0) return plane - center < half;
    if (half >= center/2) return center - half < plane;
    return center - plane < half;
}

TileSpan occupiedSpan(double center, double half, double size, int count) {
    const int begin = firstMatching(count, [&](int i) { return crosses(center, half, (i+1)*size, -1); });
    const int end = firstMatching(count, [&](int i) { return !crosses(center, half, i*size, 1); });
    return {begin, end};
}

bool clearBox(const TileMap& map, Vector2 center, Vector2 half, std::size_t& queries) {
    const double size = map.tileSize();
    if (!finite(center) || !finite(half) || half.x <= 0 || half.y <= 0 ||
        center.x < half.x || center.y < half.y ||
        map.width()*size - center.x < half.x || map.height()*size - center.y < half.y)
        return false;
    const TileSpan xs = occupiedSpan(center.x, half.x, size, map.width());
    const TileSpan ys = occupiedSpan(center.y, half.y, size, map.height());
    for (int y = ys.begin; y < ys.end; ++y) {
        for (int x = xs.begin; x < xs.end; ++x) {
            ++queries;
            if (!map.isWalkable(x, y)) return false;
        }
    }
    return true;
}

void sweepAxis(const TileMap& map, bool alongX, Vector2 half, float delta,
               TileMoveResult& result) {
    if (delta == 0) return;
    float& center = alongX ? result.position.x : result.position.y;
    bool& hit = alongX ? result.hitX : result.hitY;
    const double radius = alongX ? half.x : half.y;
    const double size = map.tileSize();
    const int count = alongX ? map.width() : map.height();
    const TileSpan occupied = occupiedSpan(center, radius, size, count);
    const TileSpan lateral = alongX
        ? occupiedSpan(result.position.y, half.y, size, map.height())
        : occupiedSpan(result.position.x, half.x, size, map.width());
    const int sign = delta > 0 ? 1 : -1;
    const double requested = double(center) + delta;
    const float target = static_cast<float>(std::clamp(requested, 0.0, count*size));
    // Include float rounding in the sweep so the stored position is legal too.
    const double travel = std::max(std::abs(double(delta)), std::abs(double(target) - center));
    for (int slice = sign > 0 ? occupied.end : occupied.begin-1; ; slice += sign) {
        const double plane = (sign > 0 ? slice : slice+1)*size;
        if (travel <= sign*(plane - center) - radius && !crosses(requested, radius, plane, sign) &&
            !crosses(target, radius, plane, sign)) break;
        bool blocked = slice < 0 || slice >= count;
        for (int other = lateral.begin; !blocked && other < lateral.end; ++other) {
            ++result.tilesQueried;
            blocked = alongX ? !map.isWalkable(slice, other) : !map.isWalkable(other, slice);
        }
        if (blocked) {
            float contact = static_cast<float>(plane - sign*radius);
            // No epsilon or skin. If the exact contact rounds inside the wall,
            // select the adjacent representable float on the legal side.
            if (crosses(contact, radius, plane, sign))
                contact = std::nextafter(contact, -sign*std::numeric_limits<float>::infinity());
            center = contact;
            hit = true;
            return;
        }
    }
    center = target;
}

} // namespace

bool canOccupyTiles(const TileMap& map, Vector2 center, Vector2 halfSize) {
    std::size_t queries = 0;
    return clearBox(map, center, halfSize, queries);
}

TileMoveResult moveAgainstTiles(const TileMap& map, Vector2 center, Vector2 halfSize,
                                Vector2 displacement) {
    TileMoveResult result{center};
    if (!finite(displacement) || !clearBox(map, center, halfSize, result.tilesQueried))
        throw std::invalid_argument("tile movement requires a legal AABB and finite displacement");
    sweepAxis(map, true, halfSize, displacement.x, result);
    sweepAxis(map, false, halfSize, displacement.y, result);
    return result;
}

} // namespace tiny2d
