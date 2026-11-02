#include "tiny2d/SceneRenderer.h"
#include <stdexcept>
#include "OpenGL.h"
#include "tiny2d/detail/SpriteGeometry.h"
namespace tiny2d {
SceneRenderer::SceneRenderer():context_(glfwGetCurrentContext()){}
void SceneRenderer::requireContext()const{detail::requireContext("scene renderer");if(glfwGetCurrentContext()!=context_)throw std::logic_error("wrong SceneRenderer context");}
void SceneRenderer::setAsset(uint32_t id,std::shared_ptr<const Texture2D> t){
    requireContext();if(!t)throw std::invalid_argument("null scene texture");t->requireCurrentContext();
    auto old=assets_.find(id);if(old!=assets_.end())old->second->requireCurrentContext();assets_[id]=std::move(t);
}
void SceneRenderer::removeAsset(uint32_t id){requireContext();auto i=assets_.find(id);if(i!=assets_.end()){i->second->requireCurrentContext();assets_.erase(i);}}
void SceneRenderer::begin(const Camera2D& c){requireContext();if(active_)throw std::logic_error("nested scene pass");batch_.beginFrame(c);active_=true;}
SceneRenderer::Prepared SceneRenderer::prepare(const Scene& scene)const {
    requireContext();
    Prepared result;
    const auto ids=scene.renderables();
    result.commands.reserve(ids.size());
    for(auto id:ids) {
        const auto* object=scene.find(id);
        if(!object||!object->enabled||!object->sprite)throw std::invalid_argument("invalid renderable handle");
        object->transform.validate();
        const auto& sprite=*object->sprite;
        const auto asset=assets_.find(sprite.asset);
        if(asset==assets_.end()||!asset->second)throw std::invalid_argument("missing scene texture asset");
        const auto& texture=asset->second;
        texture->requireCurrentContext();
        const Vector2 size{sprite.size.x*object->transform.scale.x,sprite.size.y*object->transform.scale.y};
        const Color tint{sprite.r,sprite.g,sprite.b,sprite.a};
        (void)detail::spriteCorners(object->transform.position,size,object->transform.rotation,tint);
        result.textures.try_emplace(texture.get(),texture);
        result.commands.push_back({texture.get(),object->transform.position,size,object->transform.rotation,tint});
    }
    return result;
}
void SceneRenderer::submitPrepared(Prepared prepared) {
    // Allocate the merged owner set before touching either the queue or its owners.
    auto owners=keepAlive_;
    owners.insert(prepared.textures.begin(),prepared.textures.end());
    keepAlive_.swap(owners);
    try {
        for(const auto& c:prepared.commands)batch_.draw(*c.texture,c.position,c.size,c.rotation,c.tint);
    } catch(...) {
        // Preserve the original exception. Wrong-context cleanup must not touch GL.
        try { cancel(); } catch(...) {}
        throw;
    }
}
void SceneRenderer::submit(const Scene& scene) {
    requireContext();if(!active_)throw std::logic_error("inactive scene pass");
    submitPrepared(prepare(scene)); // validation failure preserves previous submissions
}
void SceneRenderer::cancel() {
    requireContext();if(!active_)throw std::logic_error("inactive scene pass");
    batch_.cancelFrame();active_=false;keepAlive_.clear();
}
void SceneRenderer::end(){
    requireContext();if(!active_)throw std::logic_error("inactive scene pass");
    for(auto& entry:keepAlive_)entry.second->requireCurrentContext();
    try{batch_.endFrame();}catch(...){active_=false;keepAlive_.clear();throw;}
    active_=false;keepAlive_.clear();
}
void SceneRenderer::render(const Scene& scene,const Camera2D& camera) {
    requireContext();if(active_)throw std::logic_error("nested scene pass");
    auto prepared=prepare(scene); // all predictable scene errors fail before begin
    begin(camera);
    try { submitPrepared(std::move(prepared));end(); }
    catch(...) { if(active_) { try { cancel(); } catch(...) {} } throw; }
}
}
