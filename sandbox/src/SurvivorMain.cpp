#include "SurvivorDemo.h"
#include "ShaderAssets.h"
#include "tiny2d/OutputFiles.h"

#include <algorithm>
#include <charconv>
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
    if (!o.frames && (!o.capture.empty() || !o.trace.empty() || !o.metrics.empty()))
        throw std::invalid_argument("file outputs require --frames");
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
        auto options=parse(argc,argv);
        tiny2d::PackagedOutputs outputs;
        const auto files={&options.capture, &options.trace, &options.metrics};
        if (tiny2d::insideApplication(tiny2dExecutablePath())) outputs.prepare(files);
        else tiny2d::resolveNewOutputFiles(files);
        for (const char* asset : {"shaders/triangle.vert", "shaders/triangle.frag",
                "textures/sprite.png", "textures/red.png", "textures/green.png", "textures/blue.png"})
            (void)assetPath(asset);
        runSurvivor(options);
        outputs.publish();
    } catch (const std::exception& error) {
        std::cerr << "Tiny2D Survivor: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
