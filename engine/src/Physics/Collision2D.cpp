#include "tiny2d/Collision2D.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace tiny2d {
namespace { bool finite(Vector2 p){return std::isfinite(p.x)&&std::isfinite(p.y);} }
void validate(const AABB& b){if(!finite(b.min)||!finite(b.max)||b.min.x>b.max.x||b.min.y>b.max.y)throw std::invalid_argument("invalid AABB");}
void validate(const Circle& c){if(!finite(c.center)||!std::isfinite(c.radius)||c.radius<0)throw std::invalid_argument("invalid Circle");}
bool overlaps(const AABB&a,const AABB&b){validate(a);validate(b);return a.min.x<=b.max.x&&b.min.x<=a.max.x&&a.min.y<=b.max.y&&b.min.y<=a.max.y;}
bool overlaps(const Circle&a,const Circle&b){validate(a);validate(b);double x=double(a.center.x)-b.center.x,y=double(a.center.y)-b.center.y,r=double(a.radius)+b.radius;return x*x+y*y<=r*r;}
bool overlaps(const AABB&b,const Circle&c){validate(b);validate(c);double x=double(c.center.x)-std::clamp(c.center.x,b.min.x,b.max.x),y=double(c.center.y)-std::clamp(c.center.y,b.min.y,b.max.y);return x*x+y*y<=double(c.radius)*c.radius;}
bool overlaps(const Shape&a,const Shape&b){return std::visit([](auto&x,auto&y){return overlaps(x,y);},a,b);}
Shape worldShape(const Shape&s,Vector2 p){
    if(!finite(p))throw std::invalid_argument("invalid collider position");
    return std::visit([&](auto v)->Shape{validate(v);using T=decltype(v);
        if constexpr(std::is_same_v<T,AABB>){v.min=v.min+p;v.max=v.max+p;}else v.center=v.center+p;
        validate(v);return v;},s);
}
}
