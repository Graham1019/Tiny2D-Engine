#include "ParticlePresentation.h"
#include "QuadDrawing.h"
#include <algorithm>
#include <cmath>

namespace lab {
namespace {
using tiny2d::Vector2;
using tiny2d::Color;
using namespace quad;
constexpr float pi=3.14159265f;
constexpr Color paper{.87f,.93f,.96f,1}, muted{.51f,.65f,.71f,1}, teal{.38f,.92f,.84f,1};
constexpr Color fieldInk[]={{.29f,.84f,.86f,1},{1,.59f,.29f,1},{.84f,.58f,1,1},{.58f,.83f,.49f,1}};
Vector2 bounded(Vector2 p) {
    return {std::clamp(p.x,-ParticleSimulation::halfWidth,ParticleSimulation::halfWidth),
            std::clamp(p.y,-ParticleSimulation::halfHeight,ParticleSimulation::halfHeight)};
}
Vector2 position(const ParticleSimulation& sim,const ParticleEditor& edit) {
    if (edit.selected<0) return edit.pending.emitter.value_or(sim.emitter()).position;
    return edit.pending.fields.value_or(sim.fields())[edit.selected].position;
}
void move(ParticleEditor& edit,const ParticleSimulation& sim,Vector2 p) {
    if (edit.selected<0) {
        if (!edit.pending.emitter) edit.pending.emitter=sim.emitter();
        edit.pending.emitter->position=bounded(p);
    } else {
        if (!edit.pending.fields) edit.pending.fields=sim.fields();
        (*edit.pending.fields)[edit.selected].position=bounded(p);
    }
}
bool movable(const ParticleEditor& edit,const ParticleSimulation& sim) {
    return edit.selected<0 || edit.pending.fields.value_or(sim.fields())[edit.selected].kind!=FieldKind::Wind;
}
void arrow(Quads& q,Vector2 p,float angle,float length,float width,Color c) {
    const auto tip=polar(p,length,angle);
    segment(q,p,tip,width,c);
    segment(q,tip,polar(tip,4,angle+2.55f),width,c);
    segment(q,tip,polar(tip,4,angle-2.55f),width,c);
}
void guides(Quads& q,const ForceField2D& f,bool selected,float pixel) {
    const auto c=fieldInk[int(f.kind)];
    const float width=selected ? 1.55f*pixel : .85f*pixel;
    if (f.kind==FieldKind::Wind) {
        if (std::hypot(f.direction.x,f.direction.y)>0)
            for (int i=-2;i<=2;++i) arrow(q,{i*108.f,-147},std::atan2(f.direction.y,f.direction.x),13,width,c);
        return;
    }
    // Dashed arcs expose the compact support without painting over the particle cores.
    for (int i=0;i<96;i+=2) segment(q,polar(f.position,f.radius,i*pi/48),
                                     polar(f.position,f.radius,(i+1)*pi/48),width,{c.r,c.g,c.b,.38f});
    ring(q,f.position,selected ? 7 : 5,4,pi/4,width,c);
    for (int i=0;i<4;++i) {
        const float a=i*pi*.5f;
        const bool vortex=f.kind==FieldKind::Vortex;
        auto p=polar(f.position,vortex?17.f:14.f,a);
        const float heading=vortex ? a+(f.clockwise ? .5f : -.5f)*pi : a+(f.kind==FieldKind::Attractor?pi:0);
        arrow(q,p,heading,8,width,c);
    }
}
}
const char* fieldName(FieldKind kind) {
    constexpr const char* names[]={"Attractor","Repulsor","Vortex","Uniform wind"};
    return names[int(kind)];
}
tiny2d::Camera2D particleCamera(tiny2d::Size logical) {
    tiny2d::Camera2D camera(logical.width,logical.height);
    camera.setZoom(std::min(logical.width/720.f,logical.height/450.f));
    return camera;
}
void ParticleEditor::pointer(const RoutedInput& input,const ParticleSimulation& sim,const tiny2d::Camera2D& camera,
                             tiny2d::Size size,View view) {
    if (!dragEnabled || !movable(*this,sim) || !input.heldCursor || (view!=View::Final && view!=View::Scene)) {
        dragging=false;
        return;
    }
    if (input.click) {
        const auto world=camera.viewportToWorld(*input.click), center=position(sim,*this);
        dragOffset={center.x-world.x,center.y-world.y};
        dragging=std::hypot(dragOffset.x,dragOffset.y)<=18.f/camera.zoom();
    }
    const auto cursor=*input.heldCursor;
    if (dragging && cursor.x>=0 && cursor.y>=0 && cursor.x<size.width && cursor.y<size.height) {
        const auto world=camera.viewportToWorld(cursor);
        move(*this,sim,{world.x+dragOffset.x,world.y+dragOffset.y});
    }
}
ParticleTickInput ParticleEditor::takeTick(const ParticleSimulation& sim,Motion motion) {
    if ((motion.x || motion.y) && movable(*this,sim)) {
        const auto p=position(sim,*this);
        const float distance=motion.x && motion.y ? 1.5f*.70710678f : 1.5f;
        move(*this,sim,{p.x+motion.x*distance,p.y+motion.y*distance});
    }
    auto result=pending;
    pending={};
    return result;
}
ParticleTickInput particleDemoTick(const ParticleSimulation& sim) {
    ParticleTickInput input;
    if (sim.tick()%180!=0) return input;
    auto fields=sim.fields();
    auto emitter=sim.emitter();
    const unsigned phase=(sim.tick()/180)%5;
    for (unsigned i=0;i<fields.size();++i) fields[i].enabled=i<4 && (phase==4 || phase==i);
    // Reconfigure each exhibit at the tick boundary, including the injection angle and tint.
    constexpr ParticleRGB palette[]={{.32f,1.f,1.f},{1.f,.56f,.26f},{.85f,.45f,1.f},{.48f,1.f,.58f},{.65f,.85f,1.f}};
    const Vector2 origins[]={{-76,10},{-135,78},{-126,-38},{-144,0},{}};
    constexpr float directions[]={-12,-25,70,0,0};
    constexpr float spreads[]={32,25,36,100,180};
    emitter.position=origins[phase]; emitter.direction=directions[phase];
    emitter.spread=spreads[phase]; emitter.color=palette[phase];
    fields[3].strength=phase==3?90.f:14.f;
    input.fields=fields; input.emitter=emitter; input.burst=128;
    return input;
}
void prepareParticles(Picture& p,const ParticleSimulation& sim,const ParticleEditor& edit,const Options& o,
                      bool paused,tiny2d::Size logical) {
    const auto& d=o.display;
    p.worldCamera=particleCamera(logical);
    p.hudCamera.setViewportSize(logical.width,logical.height);
    p.hudCamera.setPosition(logical.width*.5f,logical.height*.5f);
    p.world.clear(); p.albedo.clear(); p.debug.clear(); p.hud.clear();
    auto& surface=d.lighting ? p.albedo : p.world;
    rect(surface,-320,-160,640,320,{.012f,.026f,.038f,1});
    const Color grid{.027f,.055f,.071f,1}, border{.067f,.14f,.16f,1};
    for (int x=-320;x<=320;x+=32) rect(surface,x,-160,.7f,320,x%160 ? grid : border);
    for (int y=-160;y<=160;y+=32) rect(surface,-320,y,640,.7f,y%160 ? grid : border);
    if (edit.guides && (d.view==View::Final || d.view==View::Scene)) {
        for (unsigned i=0;i<sim.fields().size();++i) {
            const auto& f=sim.fields()[i];
            if (f.enabled) guides(p.debug,f,int(i)==edit.selected,1/p.worldCamera.zoom());
        }
        ring(p.debug,sim.emitter().position,10,4,0,1.1f,muted);
    }
    if (!d.hud) return;
    const float scale=std::clamp(logical.width/740.f,.8f,1.7f);
    const float x=16*scale, bottom=logical.height-37*scale;
    rect(p.hud,0,0,logical.width,34*scale,{.035f,.06f,.073f,1});
    rect(p.hud,x,9*scale,3*scale,15*scale,teal);
    lettering(p.hud,"FORCEFIELD",x+10*scale,9*scale,2*scale,paper);
    std::string label=o.particleDemo ? "DEMO  " : "LIVE  ";
    const char* stages[]={"ATTRACTOR","REPULSOR","VORTEX","WIND","ENSEMBLE"};
    label+=o.particleDemo ? stages[sim.tick() ? ((sim.tick()-1)/180)%5 : 0] : "PARTICLE PLAYGROUND";
    lettering(p.hud,label,x+145*scale,9*scale,scale,teal);
    const char* views[]={"SCENE","BRIGHT","BLUR","FINAL","LIGHT"};
    lettering(p.hud,std::string("BLOOM ")+(d.bloom?"ON":"OFF")+"   "+views[int(d.view)],x+145*scale,20*scale,scale,muted);
    rect(p.hud,0,bottom,logical.width,37*scale,{.035f,.06f,.073f,1});
    lettering(p.hud,std::string(paused?"PAUSED  ":"TICK  ")+std::to_string(sim.tick())+
              "    ACTIVE "+std::to_string(sim.active())+" / "+std::to_string(sim.capacity())+
              "    DROPPED "+std::to_string(sim.dropped()),x,bottom+6*scale,scale,paper);
    if (edit.notice.empty()) {
        constexpr const char* labels[]={"ATTRACT","REPEL","VORTEX","WIND"};
        float cursor=x;
        for (unsigned k=0;k<4;++k) {
            const bool enabled=std::any_of(sim.fields().begin(),sim.fields().end(),[&](const auto& field) {
                return field.enabled && unsigned(field.kind)==k;
            });
            const auto c=enabled ? fieldInk[k] : Color{.35f,.40f,.43f,1};
            rect(p.hud,cursor,bottom+23*scale,4*scale,4*scale,c);
            lettering(p.hud,labels[k],cursor+9*scale,bottom+21*scale,scale,c);
            cursor+=65*scale;
        }
    } else lettering(p.hud,edit.notice,x,bottom+21*scale,scale,muted);
}
}
