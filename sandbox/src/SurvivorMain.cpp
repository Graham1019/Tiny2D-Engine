#include "SurvivorDemo.h"
#include "ShaderAssets.h"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
unsigned number(const std::string& text,unsigned max) {
    unsigned n=0;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),n);
    if (parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || n<1 || n>max)
        throw std::invalid_argument("integer outside 1.."+std::to_string(max));
    return n;
}
SurvivorOptions parse(int argc,char** argv) {
    SurvivorOptions o;
    bool bloomSet=false;
    std::vector<std::string> seen;
    for (int i=1;i<argc;i+=2) {
        if (i+1>=argc || !*argv[i+1]) throw std::invalid_argument("missing option value");
        const std::string key=argv[i],value=argv[i+1];
        if (std::find(seen.begin(),seen.end(),key)!=seen.end()) throw std::invalid_argument("repeated "+key);
        seen.push_back(key);
        if (key=="--presentation" && (value=="enhanced" || value=="classic")) o.enhanced=value=="enhanced";
        else if (key=="--bloom" && (value=="on" || value=="off")) { o.bloom=value=="on"; bloomSet=true; }
        else if (key=="--frames") o.frames=number(value,1000000);
        else if (key=="--difficulty") o.difficulty=number(value,5);
        else if (key=="--capture") o.capture=value;
        else if (key=="--trace") o.trace=value;
        else if (key=="--metrics") o.metrics=value;
        else throw std::invalid_argument("unsupported argument "+key+" "+value);
    }
    if (bloomSet && !o.enhanced) throw std::invalid_argument("--bloom requires enhanced presentation");
    std::vector<std::filesystem::path> paths;
    for (auto* field:{&o.capture,&o.trace,&o.metrics}) {
        auto& path=*field;
        if (path.empty()) continue;
        if (!o.frames) throw std::invalid_argument("file outputs require --frames");
        const auto absolute=std::filesystem::weakly_canonical(std::filesystem::absolute(path));
        if (std::find(paths.begin(),paths.end(),absolute)!=paths.end() ||
            std::filesystem::exists(std::filesystem::symlink_status(path)))
            throw std::invalid_argument("outputs require distinct new paths: "+path);
        paths.push_back(absolute);
        path=absolute.string(); // GLFW can change cwd to the bundle Resources directory.
    }
    return o;
}
}
int main(int argc,char** argv) {
    try {
        if (argc==2 && std::string(argv[1])=="--help") {
            std::cout<<"Tiny2DSurvivor [--presentation classic|enhanced] [--bloom on|off] "
                        "[--difficulty 1..5] [--frames N] [--capture NEW.png] [--trace NEW.csv] [--metrics NEW.csv]\n";
            return 0;
        }
        const auto options=parse(argc,argv);
        for (const char* asset : {"shaders/triangle.vert", "shaders/triangle.frag",
                "textures/sprite.png", "textures/red.png", "textures/green.png", "textures/blue.png"})
            (void)assetPath(asset);
        runSurvivor(options);
    } catch (const std::exception& error) {
        std::cerr << "Tiny2D Survivor: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
