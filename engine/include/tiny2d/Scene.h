#pragma once
#include "tiny2d/GameObject.h"
#include <functional>
#include <memory>
#include <set>
#include <vector>
namespace tiny2d {
enum class CollisionPhase { Enter,Stay,Exit };
struct CollisionEvent { ObjectId a,b; CollisionPhase phase; };
class Scene {
public:
    Scene();
    Scene(const Scene&)=delete;
    Scene& operator=(const Scene&)=delete;
    ObjectId create();
    bool destroy(ObjectId);
    GameObject* find(ObjectId);
    const GameObject* find(ObjectId)const;
    void clear();
    std::size_t count()const;
    std::vector<ObjectId> objects()const;
    std::vector<ObjectId> renderables()const;
    using Update=std::function<void(Scene&,ObjectId,double)>;
    using Events=std::function<void(Scene&,const CollisionEvent&)>;
    void update(double dt,const Update& update={},const Events& events={});
    std::size_t contacts()const{return contacts_.size();}
private:
    const uint64_t sceneId_;
    struct Slot { uint32_t generation=1; std::unique_ptr<GameObject> object; bool dead=false; };
    std::vector<Slot> slots_;
    std::vector<ObjectId> order_;
    using Pair=std::pair<ObjectId,ObjectId>;
    std::set<Pair> contacts_;
    bool updating_=false;
    void collect();
};
}
