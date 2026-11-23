#pragma once
#include "tiny2d/Scene.h"

namespace tiny2d {
struct DebugLine { Vector2 from, to; };
void outline(const Shape& world, std::vector<DebugLine>& lines);
bool outlineCollider(const Scene&, ObjectId, std::vector<DebugLine>&);
}
