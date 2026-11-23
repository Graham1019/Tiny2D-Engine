#include "M5Test.h"
#include "Neon.h"
#include <initializer_list>
#include <limits>

using namespace lab;
namespace {
Options options(std::initializer_list<const char*> args) {
    return parse(static_cast<int>(args.size()),args.begin());
}
void arguments() {
    const auto baseline=options({"lab"}).display;
    check(!baseline.lighting && baseline.view==View::Final,"legacy display defaults");
    const auto d=options({"lab","--view","light","--ambient",".375","--lighting","on"}).display;
    check(d.lighting && d.ambient==.375f && d.view==View::Light,"lighting CLI is order independent");
    check(options({"lab","--ambient","2e0"}).display.ambient==2,"upper inclusive");
    check(options({"lab","--ambient","0"}).display.ambient==0,"lower inclusive");
    for (auto value:{"nan","inf","-inf","NaN","-1","-0","2.00001","0.2abc","1e500","0x1p-1","", " 1"})
        rejects([&]{options({"lab","--ambient",value});});
    rejects([]{options({"lab","--ambient"});});
    rejects([]{options({"lab","--lighting"});});
    rejects([]{options({"lab","--lighting","yes"});});
    rejects([]{options({"lab","--view","light"});});
    rejects([]{options({"lab","--lighting","on","--view","light","--lighting","off"});});
    for (auto member:{&Display::ambient,&Display::lightStrength,&Display::radiusMultiplier}) {
        Display invalid;
        invalid.*member=std::numeric_limits<float>::quiet_NaN();
        rejects([&]{invalid.validate();});
        invalid.*member=std::numeric_limits<float>::infinity();
        rejects([&]{invalid.validate();});
        invalid.*member=-1;
        rejects([&]{invalid.validate();});
    }
    for (auto member:{&Display::lightStrength,&Display::radiusMultiplier}) {
        Display invalid;
        invalid.*member=5;
        rejects([&]{invalid.validate();});
    }
    Display invalid;
    invalid.radiusMultiplier=0;
    rejects([&]{invalid.validate();});
}
void routing() {
    Controls controls;
    Display d;
    Keys both;
    both.set(ToggleLighting).set(ShowLight);
    auto step=[&](Keys keys={},bool capture=false,Commands commands={}) {
        return controls.poll(keys,true,1./60,d,capture,commands);
    };
    check(step(both,true).steps==1 && !d.lighting,"captured light keys preserve fixed tick");
    check(step(both).steps==1 && !d.lighting && d.view==View::Final,"release gate after text capture");
    step();
    step(both);
    check(d.lighting && d.view==View::Light,"fresh toggle and diagnostic view");
    step(both);
    check(d.lighting,"no repeated toggle while held");
    step();
    Keys toggle; toggle.set(ToggleLighting);
    step(toggle);
    check(!d.lighting && d.view==View::Final,"turning lights off restores final view");
    step();
    Keys show; show.set(ShowLight);
    step(show);
    check(d.view==View::Final,"unavailable Light view ignored");
    controls.poll({},false,0,d);
    check(step(both).steps==0 && !d.lighting,"focus recovery cannot replay light keys");
    step();
    Keys pause; pause.set(Pause);
    step(pause);
    check(controls.paused,"paused");
    Commands ui;
    ui.edges=toggle; ui.view=View::Light;
    ui.ambient=.7f; ui.lightStrength=2; ui.radiusMultiplier=.5f;
    check(!step({},false,ui).steps && d.lighting && d.view==View::Light && d.ambient==.7f &&
          d.lightStrength==2 && d.radiusMultiplier==.5f,"paused UI lighting adjustments");
    ui={}; ui.edges.set(Step);
    check(step({},false,ui).steps==1,"one fixed step while lit");
    ui={}; ui.edges.set(Reset);
    check(step({},false,ui).reset && d.lighting && d.ambient==.7f && d.view==View::Light &&
          d.radiusMultiplier==.5f && d.lightStrength==2,"reset preserves display settings");
}
}
int main() {
    try { arguments(); routing(); }
    catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
