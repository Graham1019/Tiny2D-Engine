#include "SurvivorDemo.h"
#include "ShaderAssets.h"
#include <exception>
#include <iostream>
int main() {
    try {
        // Validate the shipped resource contract before creating the window.
        for (const char* asset : {"shaders/triangle.vert", "shaders/triangle.frag",
                "textures/sprite.png", "textures/red.png", "textures/green.png", "textures/blue.png"})
            (void)assetPath(asset);
        runSurvivor(0);
    } catch (const std::exception& error) {
        std::cerr << "Tiny2D Survivor: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
