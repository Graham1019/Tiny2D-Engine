#include "tiny2d/DebugDraw.h"
#include <cmath>

namespace tiny2d {
void outline(const Shape& shape, std::vector<DebugLine>& lines) {
    if (auto box = std::get_if<AABB>(&shape)) {
        validate(*box);
        Vector2 topRight{box->max.x, box->min.y}, bottomLeft{box->min.x, box->max.y};
        lines.insert(lines.end(), {{box->min,topRight},{topRight,box->max},
                                   {box->max,bottomLeft},{bottomLeft,box->min}});
        return;
    }
    const auto& circle = std::get<Circle>(shape);
    validate(circle);
    constexpr int sides = 40;
    Vector2 first{circle.center.x + circle.radius,circle.center.y}, previous=first;
    for (int i=1; i<=sides; ++i) {
        const float a=i*6.283185307f/sides;
        Vector2 next=i==sides ? first : Vector2{circle.center.x+circle.radius*std::cos(a),
                                                circle.center.y+circle.radius*std::sin(a)};
        lines.push_back({previous,next});
        previous=next;
    }
}
bool outlineCollider(const Scene& scene, ObjectId id, std::vector<DebugLine>& lines) {
    const auto* object=scene.find(id);
    if (!object || !object->enabled || !object->collider) return false;
    outline(worldShape(*object->collider,object->transform.position),lines);
    return true;
}
}
