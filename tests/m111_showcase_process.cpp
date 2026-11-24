#include "M5Test.h"
#include "ShowcaseProcess.h"
#include <cerrno>
#include <fstream>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>

using namespace lab;
namespace fs=std::filesystem;
namespace {
class Workspace {
public:
    Workspace() {
        auto name=(fs::temp_directory_path()/"showcase-process-XXXXXX").string();
        if (!mkdtemp(name.data())) throw std::runtime_error("create process fixture");
        path=name;
    }
    ~Workspace() { std::error_code error; fs::remove_all(path,error); }
    fs::path path;
};
ChildExit wait(ExhibitProcess& process) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while (std::chrono::steady_clock::now()<deadline) {
        if (auto result=process.poll()) return *result;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    throw std::runtime_error("child failed to finish");
}
void reaped(pid_t pid) {
    int status=0;
    check(::waitpid(pid,&status,WNOHANG)==-1 && errno==ECHILD,"child was reaped exactly once");
}
}

int main(int argc,char** argv) {
    if (argc==5 && std::string(argv[1])=="--echo") {
        std::ofstream stream(argv[2]);
        stream<<argv[3]<<'\n'<<argv[4];
        return stream?0:1;
    }
    if (argc==2 && std::string(argv[1])=="--exit7") return 7;
    if (argc==2 && std::string(argv[1])=="--wait") {
        for (;;) ::pause();
    }
    try {
        Workspace fixture;
        const auto dir=fixture.path/"离线 Gallery";
        fs::create_directory(dir);
        const auto exe=dir/"Child program";
        fs::copy_file(fs::canonical(argv[0]),exe);
        const auto output=dir/"参数 spaces.txt";
        const std::string first="a filename & 'quotes'",last="第一个展品";
        ExhibitProcess process;
        rejects([&]{process.start({dir/"absent",{}});});
        const auto echo=process.start({exe,{"--echo",output.string(),first,last}});
        rejects([&]{process.start({exe,{"--exit7"}});});
        check(wait(process).code==0,"child exited successfully");
        reaped(echo);
        std::ifstream read(output);
        std::string a,b;
        std::getline(read,a); std::getline(read,b);
        check(a==first && b==last,"every UTF-8/space argument reached child unchanged");
        const auto again=process.start({exe,{"--exit7"}});
        check(wait(process).code==7,"nonzero exit retained for launcher diagnostic");
        reaped(again);
        const auto stopped=process.start({exe,{"--wait"}});
        process.close();
        reaped(stopped);
        check(!process.running() && !process.poll(),"closed process idle");
        pid_t abandoned;
        {
            ExhibitProcess owner;
            abandoned=owner.start({exe,{"--wait"}});
        }
        reaped(abandoned);

        const auto build=dir/"bin/lab/tiny2d_lab";
        const auto bundle=dir/"Tiny2D Showcase.app/Contents/MacOS/tiny2d_lab";
        const auto demo=dir/"安装 数据/parent.replay";
        const auto stem=dir/"captures/one run";
        for (const auto& e:exhibits()) {
            const auto command=exhibitCommand(e.id,build,demo,2,stem);
            check(findExhibit(e.key).id==e.id,"catalog ID roundtrips");
            check(!command.arguments.empty(),"launch command contains a preset");
            bool survivor=e.id==ExhibitId::Classic || e.id==ExhibitId::Enhanced;
            const auto package=exhibitCommand(e.id,bundle,demo);
            check(package.executable==(survivor?dir/"Tiny2D Survivor.app/Contents/MacOS/Tiny2DSurvivor":bundle),
                  "packaged helpers resolve next to app");
            check(command.executable==(survivor?dir/"bin/Tiny2DSurvivor.app/Contents/MacOS/Tiny2DSurvivor":build),
                  "build helpers resolve relative to lab executable");
        }
        check(tourVisits("all").size()==8 && tourVisits("neon,shaders,neon").size()==3,"finite tour supports reopening");
        rejects([&]{tourVisits("neon,unknown");});
        rejects([&]{tourVisits("neon,");});
        std::cout<<"Spawn argv, unicode layout, duplicate launch, child exit, owner close/reap and reopen passed\n";
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
