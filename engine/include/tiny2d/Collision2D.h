#pragma once
#include "tiny2d/Vector2.h"
#include <variant>
namespace tiny2d {
struct AABB { Vector2 min,max; };
struct Circle { Vector2 center; float radius=0; };
using Shape=std::variant<AABB,Circle>;
void validate(const AABB&);
void validate(const Circle&);
bool overlaps(const AABB&,const AABB&);
bool overlaps(const Circle&,const Circle&);
bool overlaps(const AABB&,const Circle&);
inline bool overlaps(const Circle& c,const AABB& b){return overlaps(b,c);}
bool overlaps(const Shape&,const Shape&);
// Local offsets translated by position only. Rotation and scale are visual only.
Shape worldShape(const Shape&,Vector2 position);
}
