#include "tiny2d/Scene.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <stdexcept>
namespace tiny2d {
namespace {
uint64_t allocateSceneId() {
    static std::atomic<uint64_t> next{1};
    auto value=next.load(std::memory_order_relaxed);
    for (;;) {
        if(value==UINT64_MAX)throw std::overflow_error("Scene identity exhausted");
        if(next.compare_exchange_weak(value,value+1,std::memory_order_relaxed))return value;
    }
}
}
Scene::Scene():sceneId_(allocateSceneId()){}
ObjectId Scene::create(){
    auto object=std::make_unique<GameObject>();
    std::size_t i=0;for(;i<slots_.size();++i)if(!slots_[i].object&&slots_[i].generation!=UINT32_MAX)break;
    if(i==UINT32_MAX)throw std::length_error("Scene slot limit");
    if(i==slots_.size())slots_.emplace_back();
    ObjectId id{sceneId_,static_cast<uint32_t>(i),slots_[i].generation};
    object->id_=id;
    order_.push_back(id);slots_[i].object=std::move(object);return id;
}
GameObject* Scene::find(ObjectId id){return const_cast<GameObject*>(static_cast<const Scene&>(*this).find(id));}
const GameObject* Scene::find(ObjectId id)const{
    if(id.sceneId!=sceneId_||id.index>=slots_.size())return nullptr;const auto&s=slots_[id.index];
    return s.generation==id.generation&&!s.dead?s.object.get():nullptr;
}
bool Scene::destroy(ObjectId id){
    if(!find(id))return false;slots_[id.index].dead=true;
    for(auto i=contacts_.begin();i!=contacts_.end();)if(i->first==id||i->second==id)i=contacts_.erase(i);else ++i;
    if(!updating_)collect();return true;
}
void Scene::collect(){
    order_.erase(std::remove_if(order_.begin(),order_.end(),[&](ObjectId id){return !find(id);}),order_.end());
    for(auto&s:slots_)if(s.dead){s.object.reset();s.dead=false;++s.generation;}
}
void Scene::clear(){auto ids=objects();for(auto id:ids)destroy(id);}
std::vector<ObjectId> Scene::objects()const{std::vector<ObjectId> r;for(auto id:order_)if(find(id))r.push_back(id);return r;}
std::size_t Scene::count()const{return objects().size();}
std::vector<ObjectId> Scene::renderables()const{
    auto r=objects();r.erase(std::remove_if(r.begin(),r.end(),[&](ObjectId id){auto o=find(id);return !o->enabled||!o->sprite;}),r.end());
    std::stable_sort(r.begin(),r.end(),[&](ObjectId a,ObjectId b){return find(a)->sprite->renderOrder<find(b)->sprite->renderOrder;});return r;
}
void Scene::update(double dt,const Update& callback,const Events& event){
    if(updating_)throw std::logic_error("recursive Scene update");
    if(!std::isfinite(dt)||dt<0)throw std::invalid_argument("invalid Scene delta");
    const auto ids=objects();updating_=true;
    try {
        for(auto id:ids){auto o=find(id);if(!o||!o->enabled)continue;
            if(callback)callback(*this,id,dt);
            o=find(id);if(!o||!o->enabled)continue;
            auto next=o->transform;next.position.x+=o->velocity.x*dt;next.position.y+=o->velocity.y*dt;next.validate();o->transform=next;
        }
        std::vector<std::pair<ObjectId,Shape>> shapes;
        for(auto id:ids){auto o=find(id);if(o&&o->enabled&&o->collider)shapes.emplace_back(id,worldShape(*o->collider,o->transform.position));}
        std::set<Pair> current;
        for(std::size_t i=0;i<shapes.size();++i)for(std::size_t j=i+1;j<shapes.size();++j)
            if(overlaps(shapes[i].second,shapes[j].second))current.insert(std::minmax(shapes[i].first,shapes[j].first));
        std::vector<CollisionEvent> events;
        for(auto p:current)events.push_back({p.first,p.second,contacts_.count(p)?CollisionPhase::Stay:CollisionPhase::Enter});
        for(auto p:contacts_)if(!current.count(p))events.push_back({p.first,p.second,CollisionPhase::Exit});
        contacts_=std::move(current);
        for(auto e:events)if(find(e.a)&&find(e.b)&&event)event(*this,e);
    }catch(...){updating_=false;collect();throw;}
    updating_=false;collect();
}
}
