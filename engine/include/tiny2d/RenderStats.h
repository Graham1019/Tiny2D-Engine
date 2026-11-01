#pragma once
#include <cstdint>
namespace tiny2d {
struct RenderStats {
    std::uint64_t submittedSprites=0, drawCalls=0, batchFlushes=0, textureBinds=0, uploadedVertexBytes=0;
};
}
