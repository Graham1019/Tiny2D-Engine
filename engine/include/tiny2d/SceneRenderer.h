#pragma once
#include "tiny2d/Scene.h"
#include "tiny2d/BatchRenderer.h"
#include <map>
namespace tiny2d {
// Main thread, owning live context required for all operations and destruction.
// Assets live in this adapter; Scene contains only numeric asset references.
class SceneRenderer {
public:
    SceneRenderer();
    SceneRenderer(const SceneRenderer&)=delete;
    SceneRenderer& operator=(const SceneRenderer&)=delete;
    SceneRenderer(SceneRenderer&&)=delete;
    SceneRenderer& operator=(SceneRenderer&&)=delete;
    void setAsset(uint32_t,std::shared_ptr<const Texture2D>);
    void removeAsset(uint32_t);
    void begin(const Camera2D&);
    void submit(const Scene&);
    void end();
    void render(const Scene&,const Camera2D&);
    const RenderStats& stats()const{return batch_.stats();}
private:
    std::map<uint32_t,std::shared_ptr<const Texture2D>> assets_;
    std::vector<std::shared_ptr<const Texture2D>> keepAlive_;
    BatchRenderer batch_; // destroyed before keep-alive on exceptional teardown
    bool active_=false;
    void* context_=nullptr;
    void requireContext()const;
};
}
