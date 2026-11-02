#pragma once
#include "tiny2d/Transform2D.h"
#include "tiny2d/Collision2D.h"
#include <cstdint>
#include <optional>
#include <tuple>
namespace tiny2d {
struct ObjectId {
    uint64_t sceneId=0;
    uint32_t index=UINT32_MAX,generation=0;
    friend bool operator==(ObjectId a,ObjectId b){return a.sceneId==b.sceneId&&a.index==b.index&&a.generation==b.generation;}
    friend bool operator<(ObjectId a,ObjectId b){return std::tie(a.sceneId,a.index,a.generation)<std::tie(b.sceneId,b.index,b.generation);}
};
struct SpriteData {
    uint32_t asset=0;
    Vector2 size{1,1};
    float r=1,g=1,b=1,a=1;
    int renderOrder=0;
};
class Scene;
struct GameObject {
    GameObject()=default;
    GameObject(const GameObject&)=delete;
    GameObject& operator=(const GameObject&)=delete;
    GameObject(GameObject&&)=delete;
    GameObject& operator=(GameObject&&)=delete;
    ObjectId id()const{return id_;}
    Transform2D transform;
    bool enabled=true;
    std::optional<SpriteData> sprite;
    std::optional<Shape> collider;
    Vector2 velocity{};
private:
    friend class Scene;
    ObjectId id_;
};
}
