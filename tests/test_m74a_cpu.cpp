#include "M5Test.h"
#include "Presentation.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace lab {
struct NeonProbe {
    static auto cursors(const Neon& n) { return std::make_tuple(n.rng_, n.nextBolt_, n.nextBurst_, n.nextRelay_); }
};
}

namespace {
using namespace lab;
constexpr float nan=std::numeric_limits<float>::quiet_NaN();
constexpr float inf=std::numeric_limits<float>::infinity();

void materials() {
    NormalMaterial2D tile, flat{NormalMaterialKind::Flat, 2}, zero{NormalMaterialKind::ProceduralTile, 0};
    const auto first=makeNormalTile(), second=makeNormalTile();
    check(first.width==normalTileSize && first.height==normalTileSize &&
          first.pixels.size()==4*normalTileSize*normalTileSize && first.pixels==second.pixels,
          "repeatable, complete material upload");
    for (int y=0; y<normalTileSize; ++y) {
        for (int x=0; x<normalTileSize; ++x) {
            const auto n=tile.normalAtTexel(x,y), wrapped=tile.normalAtTexel(x-normalTileSize,y+normalTileSize);
            const auto f=flat.normalAtTexel(x,y), z=zero.normalAtTexel(x,y);
            check(std::abs(std::hypot(n.x,n.y,n.z)-1)<2e-6f && n.z>0, "finite normalized surface");
            check(n.x==wrapped.x && n.y==wrapped.y && n.z==wrapped.z, "periodic sampling including negative coordinates");
            check(f.x==0 && f.y==0 && f.z==1 && z.x==0 && z.y==0 && z.z==1, "exact flat material and zero relief");
            const auto* rgba=first.pixels.data()+4*(y*normalTileSize+x);
            check(std::abs((rgba[0]-128)/127.f-n.x)<=.5f/127+1e-6f &&
                  std::abs((rgba[1]-128)/127.f-n.y)<=.5f/127+1e-6f &&
                  std::abs(rgba[2]/255.f-n.z)<=.5f/255+1e-6f && rgba[3]==255,
                  "row-major linear vector encoding within half a quantization step");
        }
    }
    const auto left=tile.normalAtTexel(5,28), right=tile.normalAtTexel(58,28);
    const auto upper=tile.normalAtTexel(28,5), lower=tile.normalAtTexel(28,58);
    check(left.x<-.65f && right.x>.65f && upper.y<-.65f && lower.y>.65f, "world-space panel bevel directions");
    check(tile.normalAtTexel(24,44).y>.3f && tile.normalAtTexel(24,19).y==0, "asymmetric groove orientation");
    tile.strength=2;
    const auto enhanced=tile.normalAtTexel(5,28);
    check(enhanced.x<left.x && std::abs(enhanced.x/enhanced.z - 2*left.x/left.z)<1e-5f, "strength scales slope");
    for (float bad:{nan,inf,-.001f,2.001f}) {
        tile.strength=bad;
        rejects([&]{ tile.normalAtTexel(0,0); });
    }
    rejects([]{ NormalMaterial2D{static_cast<NormalMaterialKind>(-1), 1}.validate(); });
}
void lighting() {
    const auto normal=NormalMaterial2D{}.normalAtTexel(5,28);
    Light2D light{{-20,0},64,{1,1,1,1},1,{},20};
    const float left=directionalFactor(light,{0,0},normal);
    check(std::abs(left-(normal.z-normal.x)/std::sqrt(2.f))<1e-5f, "left light on left bevel");
    light.position.x=20;
    const float right=directionalFactor(light,{0,0},normal);
    check(std::abs(right-std::max(0.f,(normal.z+normal.x)/std::sqrt(2.f)))<1e-5f && left>right+.5f,
          "equal-distance opposite light changes only the direction factor");
    for (int x=-100; x<=100; ++x) {
        light.position.x=float(x);
        const auto f=directionalFactor(light,{0,0},normal);
        check(f>=0 && f<=1, "bounded Lambert factor");
    }
    LightList list;
    for (float bad:{nan,inf,-1.f,0.f,1.e-6f,4097.f}) {
        light.height=bad;
        rejects([&]{ list.add(light); });
    }
    for (float limit:{1.f/1024,4096.f}) {
        light.height=limit;
        list.add(light);
    }
    check(list.size()==2, "only validated heights accepted");
    rejects([&]{ directionalFactor(light,{nan,0},normal); });
    rejects([&]{ directionalFactor(light,{0,0},{0,0,0}); });
    rejects([&]{ directionalFactor(light,{0,0},{0,0,nan}); });
}
Options cli(std::initializer_list<const char*> args) { return parse(static_cast<int>(args.size()),args.begin()); }
void arguments() {
    check(!cli({"lab","--lighting","on"}).display.normalMapping, "M7.3 remains default");
    const auto d=cli({"lab","--view","normal","--normal-strength","2","--normal-material","flat",
                      "--normal-mapping","on","--lighting","on"}).display;
    check(d.view==View::Normal && d.lighting && d.normalMapping && d.normalMaterial.kind==NormalMaterialKind::Flat &&
          d.normalMaterial.strength==2 && std::string(name(d.view))=="normal", "order-independent diagnostic options");
    check(cli({"lab","--normal-material","tile","--normal-strength","0"}).display.normalMaterial.strength==0,
          "inclusive lower bound");
    for (auto bad:{"-0","-.01","2.001","inf","nan","0.4junk",""," 1","0x1p-1","1e999"})
        rejects([&]{ cli({"lab","--normal-strength",bad}); });
    rejects([]{ cli({"lab","--normal-mapping","on"}); });
    rejects([]{ cli({"lab","--lighting","on","--view","normal"}); });
    rejects([]{ cli({"lab","--normal-mapping","yes"}); });
    rejects([]{ cli({"lab","--normal-material","wood"}); });
    rejects([]{ cli({"lab","--normal-strength"}); });
}
void routing() {
    Display d;
    d.lighting=true;
    Controls c;
    Keys keys;
    keys.set(ToggleNormal).set(ShowNormal);
    auto poll=[&](Keys down={}, bool capture=false, Commands ui={}) {
        return c.poll(down,true,1./60,d,capture,ui);
    };
    check(poll(keys,true).steps==1 && !d.normalMapping, "captured N and 6 keep the simulation ticking");
    poll(keys);
    check(!d.normalMapping && d.view==View::Final, "text capture gates held keys until release");
    poll(); poll(keys);
    check(d.normalMapping && d.view==View::Normal, "fresh N and 6 select the normal buffer");
    c.poll({},false,0,d);
    check(!poll(keys).steps && d.normalMapping, "focus recovery never replays a stale toggle");
    Commands ui;
    ui.normalStrength=1.75f;
    check(poll({},true,ui).steps==1 && d.normalMaterial.strength==1.75f, "slider drag permits fixed update");
    ui={}; ui.edges.set(Pause);
    poll({},false,ui);
    ui={}; ui.normalMaterial=NormalMaterialKind::Flat; ui.normalStrength=2;
    check(!poll({},false,ui).steps && d.normalMaterial.kind==NormalMaterialKind::Flat, "edit while paused");
    ui={}; ui.edges.set(Step);
    check(poll({},false,ui).steps==1 && !poll().steps, "one paused step");
    ui={}; ui.edges.set(Reset);
    check(poll({},false,ui).reset && d.view==View::Normal && d.normalMapping && d.normalMaterial.strength==2 &&
          d.normalMaterial.kind==NormalMaterialKind::Flat, "reset preserves diagnostic display settings");
    ui={}; ui.edges.set(ToggleNormal);
    poll({},false,ui);
    check(!d.normalMapping && d.view==View::Final, "disabling normals restores Final");
    ui={}; ui.view=View::Normal;
    poll({},false,ui);
    check(d.view==View::Final, "unavailable normal view ignored");
}
bool same(tiny2d::Vector2 a,tiny2d::Vector2 b) { return a.x==b.x && a.y==b.y; }
bool sameQuad(const Quad& a,const Quad& b) {
    return same(a.position,b.position) && same(a.size,b.size) && a.angle==b.angle && a.color.r==b.color.r &&
           a.color.g==b.color.g && a.color.b==b.color.b && a.color.a==b.color.a;
}
void simulation() {
    Neon a(71), b(71);
    Display d, baseline;
    d.lighting=baseline.lighting=true;
    Picture mapped, radial;
    for (int i=0; i<180; ++i) {
        d.normalMapping=(i%4!=0);
        d.normalMaterial.strength=(i%3)*1.f;
        mapped.prepare(a,d,false,{800,450},{1600,900},{},7);
        radial.prepare(b,baseline,false,{800,450},{1600,900},{},7);
        check(std::equal(mapped.albedo.begin(),mapped.albedo.end(),radial.albedo.begin(),radial.albedo.end(),sameQuad) &&
              std::equal(mapped.emission.begin(),mapped.emission.end(),radial.emission.begin(),radial.emission.end(),sameQuad),
              "normal mode and strength preserve albedo/emission geometry and color");
        const Motion input{i%3-1,(i/9)%3-1,true};
        a.update(input); b.update(input);
        check(a.tick==b.tick && same(a.player,b.player) && NeonProbe::cursors(a)==NeonProbe::cursors(b), "tick, RNG and indices");
        for (size_t j=0; j<a.bolts.size(); ++j) {
            const auto& x=a.bolts[j]; const auto& y=b.bolts[j];
            check(x.age==y.age && x.generation==y.generation && same(x.start,y.start) && same(x.end,y.end),
                  "bolt identity, trajectory and lifetime");
        }
        for (size_t j=0; j<a.bursts.size(); ++j) {
            const auto& x=a.bursts[j]; const auto& y=b.bursts[j];
            check(x.age==y.age && x.generation==y.generation && same(x.center,y.center) && x.angle==y.angle,
                  "burst identity, lifetime and RNG-driven angle");
        }
    }
    const LabHandle old{7,ObjectKind::Player,0,1};
    check(resolve(a,7,old).has_value(), "pre-reset handle");
    a=Neon(71);
    check(!resolve(a,8,old) && a.tick==0 && NeonProbe::cursors(a)==NeonProbe::cursors(Neon(71)),
          "reset seed, restored RNG and retired selection epoch");
}
}
int main() {
    try { materials(); lighting(); arguments(); routing(); simulation(); }
    catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
