#pragma once
#include "Inspector.h"

namespace lab {
class ShaderReloadManager;
void shaderPanel(ToolState&, Commands&, GalleryEffect&, const ShaderReloadManager&,
                 std::uint64_t tick, bool paused, const Display&, bool finite);
}
