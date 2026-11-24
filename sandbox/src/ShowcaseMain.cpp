#include <mach-o/dyld.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

// The Finder entry starts the exhibit selector. The adjacent Lab helper retains
// its own CLI/defaults and can launch other scenes without returning here.
int main(int argc, char** argv) {
    try {
        std::uint32_t length = 0;
        _NSGetExecutablePath(nullptr,&length);
        std::vector<char> path(length);
        if (_NSGetExecutablePath(path.data(), &length))
            throw std::runtime_error("cannot resolve Showcase executable");
        auto helper = (std::filesystem::canonical(path.data()).parent_path()/"tiny2d_lab").string();
        std::vector<std::string> args{helper};
        if (!(argc == 2 && std::string(argv[1]) == "--help"))
            args.insert(args.end(), {"--scene", "showcase"});
        for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
        std::vector<char*> native;
        for (auto& arg : args) native.push_back(arg.data());
        native.push_back(nullptr);
        ::execv(helper.c_str(), native.data());
        throw std::runtime_error("launch packaged Lab: " + std::string(std::strerror(errno)));
    } catch (const std::exception& error) {
        std::cerr << "Tiny2D Showcase: " << error.what() << '\n';
        return 1;
    }
}
