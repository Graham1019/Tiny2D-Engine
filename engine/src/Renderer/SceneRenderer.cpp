#include "tiny2d/SceneRenderer.h"
#include <stdexcept>
#include "OpenGL.h"
namespace tiny2d {
SceneRenderer::SceneRenderer():context_(glfwGetCurrentContext()){}
void SceneRenderer::requireContext()const{detail::requireContext("scene renderer");if(glfwGetCurrentContext()!=context_)throw std::logic_error("wrong SceneRenderer context");}
void SceneRenderer::setAsset(uint32_t id,std::shared_ptr<const Texture2D> t){
    requireContext();if(!t)throw std::invalid_argument("null scene texture");t->requireCurrentContext();
    auto old=assets_.find(id);if(old!=assets_.end())old->second->requireCurrentContext();assets_[id]=std::move(t);
}
void SceneRenderer::removeAsset(uint32_t id){requireContext();auto i=assets_.find(id);if(i!=assets_.end()){i->second->requireCurrentContext();assets_.erase(i);}}
void SceneRenderer::begin(const Camera2D& c){requireContext();if(active_)throw std::logic_error("nested scene pass");batch_.beginFrame(c);active_=true;}
void SceneRenderer::submit(const Scene& s){
    requireContext();if(!active_)throw std::logic_error("inactive scene pass");
    for(auto id:s.renderables()){
        const auto& o=*s.find(id);o.transform.validate();const auto& sprite=*o.sprite;
        auto texture=assets_.at(sprite.asset);texture->requireCurrentContext();
        keepAlive_.push_back(texture);
        batch_.draw(*texture,o.transform.position,{sprite.size.x*o.transform.scale.x,sprite.size.y*o.transform.scale.y},o.transform.rotation,{sprite.r,sprite.g,sprite.b,sprite.a});
    }
}
void SceneRenderer::end(){
    requireContext();if(!active_)throw std::logic_error("inactive scene pass");
    // Validate before end: a wrong-context failure must retain the pending owners.
    for(auto&t:keepAlive_)t->requireCurrentContext();
    try{batch_.endFrame();}catch(...){active_=false;keepAlive_.clear();throw;}
    active_=false;keepAlive_.clear();
}
void SceneRenderer::render(const Scene&s,const Camera2D&c){begin(c);try{submit(s);}catch(...){end();throw;}end();}
}
