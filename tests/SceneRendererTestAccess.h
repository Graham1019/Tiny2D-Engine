#pragma once
#include "tiny2d/SceneRenderer.h"
namespace tiny2d {
// Test/benchmark-only observation; no public renderer metrics or mutable access.
struct SceneRendererTestAccess {
    static std::size_t owners(const SceneRenderer& r){return r.keepAlive_.size();}
    static std::size_t prepare(const SceneRenderer& r,const Scene& s){return r.prepare(s).textures.size();}
};
}
