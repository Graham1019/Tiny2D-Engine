#pragma once
#include "tiny2d/SceneRenderer.h"
namespace tiny2d_ui {
// Requires the renderer owning context throughout the scope. Context loss is NOT
// recoverable here: restore that context and explicitly cancel before reuse/destruction.
class RenderPass {
public:
    RenderPass(tiny2d::SceneRenderer& renderer,const tiny2d::Camera2D& camera):renderer_(renderer) { renderer_.begin(camera); }
    RenderPass(const RenderPass&)=delete;
    RenderPass& operator=(const RenderPass&)=delete;
    void end() { renderer_.end();finished_=true; }
    ~RenderPass() noexcept {
        if(!finished_&&renderer_.passActive()) {
            try { renderer_.cancel(); } catch(...) { /* Preserve the original exception. */ }
        }
    }
private:
    tiny2d::SceneRenderer& renderer_;
    bool finished_=false;
};
}
