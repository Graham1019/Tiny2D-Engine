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
    // Preflight failure preserves the active pass; runtime draw failure cancels it.
    void submit(const Scene&);
    void end();
    // Drops only pending commands. Requires the owning context.
    void cancel();
    // Validates all scene commands before begin. No GPU transaction guarantee.
    void render(const Scene&,const Camera2D&);
    const RenderStats& stats()const{return batch_.stats();}
private:
    friend struct SceneRendererTestAccess;
    struct Command {
        const Texture2D* texture; // owned by Prepared/keepAlive_, never sole ownership
        Vector2 position,size;
        float rotation;
        Color tint;
    };
    using Owners=std::map<const Texture2D*,std::shared_ptr<const Texture2D>>;
    struct Prepared { std::vector<Command> commands; Owners textures; };
    Prepared prepare(const Scene&)const;
    void submitPrepared(Prepared);
    std::map<uint32_t,std::shared_ptr<const Texture2D>> assets_;
    Owners keepAlive_;
    BatchRenderer batch_; // destroyed before keep-alive on exceptional teardown
    bool active_=false;
    void* context_=nullptr;
    void requireContext()const;
};
}
