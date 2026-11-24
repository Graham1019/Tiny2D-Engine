#include "M5Test.h"
#include "ShaderWorkspace.h"
#include <fstream>
#include <future>
#include <unistd.h>

namespace fs=std::filesystem;
using namespace lab;

namespace {
struct Scratch {
    fs::path root;
    Scratch() {
        auto pattern=(fs::temp_directory_path()/"tiny2d-shaders-XXXXXX").string();
        if (!mkdtemp(pattern.data())) throw std::runtime_error("create workspace fixture");
        root=fs::canonical(pattern);
    }
    ~Scratch() { std::error_code error; fs::remove_all(root,error); }
};
void put(const fs::path& path,const std::string& value) { std::ofstream(path)<<value; }
std::string get(const fs::path& path) { std::ifstream file(path); return {std::istreambuf_iterator<char>(file),{}}; }
}

int main() {
    try {
        Scratch tmp;
        const auto bundle=tmp.root/"演示 Demo.app",defaults=bundle/"Contents/Resources/assets/shaders/gallery";
        fs::create_directories(defaults);
        for (const auto* name:{"screen.vert","aurora.frag","orbitals.frag","contours.frag"}) put(defaults/name,name);
        const auto dest=tmp.root/"用户 编辑";
        fs::create_directory(dest);
        put(dest/"aurora.frag","preserve this edit");
        auto one=std::async(std::launch::async,[&]{return prepareShaderWorkspace(dest,defaults);});
        auto two=std::async(std::launch::async,[&]{return prepareShaderWorkspace(dest,defaults);});
        check(one.get()==dest && two.get()==dest,"concurrent first use resolves same external path");
        check(get(dest/"aurora.frag")=="preserve this edit" && get(dest/"screen.vert")=="screen.vert",
              "existing edits preserved and missing defaults copied");
        fs::remove(dest/"contours.frag");
        prepareShaderWorkspace(dest,defaults);
        check(!fs::exists(dest/"contours.frag"),"intentional deletion survives restart");
        rejects([&]{prepareShaderWorkspace(defaults,defaults);});
        rejects([&]{prepareShaderWorkspace(bundle/"Contents/Resources/work",defaults);});
        fs::create_directory_symlink(defaults,tmp.root/"alias");
        rejects([&]{prepareShaderWorkspace(tmp.root/"alias/inside",defaults);});
        fs::create_symlink(defaults/"aurora.frag",dest/"contours.frag");
        rejects([&]{prepareShaderWorkspace(dest,defaults);});
        fs::remove(dest/"contours.frag");
        fs::create_hard_link(defaults/"orbitals.frag",dest/"contours.frag");
        rejects([&]{prepareShaderWorkspace(dest,defaults);});
        check(get(defaults/"orbitals.frag")=="orbitals.frag","installed shader intact");
        rejects([&]{requireExternalOutput(bundle/"Contents/Resources/new.snap");});
        check(!insideApplication(tmp.root/"my application previews"),"ordinary data directory allowed");
        std::cout<<"Concurrent external shader initialization, UTF-8 paths, no-clobber edits, deletions, "
                    "hardlink/symlink and signed resource exclusion passed\n";
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
