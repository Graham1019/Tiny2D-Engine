#include "QuadDrawing.h"
#include "ui/BitmapFont.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <string>

namespace lab {
namespace quad {
using tiny2d::Color;
using tiny2d::Vector2;
constexpr float pi = 3.14159265f;
void rect(Quads& out,float x,float y,float w,float h,Color c,float angle) {
    out.push_back({{x,y},{w,h},angle,c});
}
void segment(Quads& out,Vector2 a,Vector2 b,float width,Color c) {
    const float dx=b.x-a.x, dy=b.y-a.y, length=std::hypot(dx,dy);
    if (length>0) rect(out,(a.x+b.x-length)/2,(a.y+b.y-width)/2,length,width,c,std::atan2(dy,dx));
}
Vector2 polar(Vector2 center,float radius,float angle) {
    return {center.x+radius*std::cos(angle),center.y+radius*std::sin(angle)};
}
void ring(Quads& out,Vector2 p,float radius,int sides,float angle,float width,Color c) {
    for (int i=0;i<sides;++i)
        segment(out,polar(p,radius,angle+i*2*pi/sides),polar(p,radius,angle+(i+1)*2*pi/sides),width,c);
}
void lettering(Quads& out,const std::string& text,float x,float y,float scale,Color c) {
    for (char ch : text) {
        auto rows=tiny2d_ui::glyphRows(ch);
        for (int row=0;row<7;++row) {
            int start=-1;
            for (int col=0;col<=5;++col) {
                const bool ink=col<5 && (rows[row] & (16u>>col));
                if (ink && start<0) start=col;
                if (!ink && start>=0) {
                    rect(out,x+start*scale,y+row*scale,(col-start)*scale,scale,c);
                    start=-1;
                }
            }
        }
        x+=6*scale;
    }
}
}
namespace {
using namespace quad;
constexpr Color grid{.012f,.029f,.044f,1}, line{.035f,.096f,.12f,1};
constexpr Color teal{.045f,2.3f,2.9f,1}, amber{5.8f,1.4f,.15f,1};
constexpr Color paper{.8f,.89f,.92f,1}, muted{.49f,.63f,.69f,1};
// Legacy drawing aliases both streams to world, preserving the original painter order.
void arena(Quads& out,Quads& glow,bool lit) {
    const Color floorInk=lit ? Color{.14f,.16f,.19f,1} : Color{.005f,.012f,.022f,1};
    const Color gridInk=lit ? Color{.24f,.30f,.33f,1} : grid;
    const Color trim=lit ? Color{.34f,.42f,.48f,1} : line;
    rect(out,Picture::floorOrigin.x,Picture::floorOrigin.y,Picture::floorSize.x,Picture::floorSize.y,floorInk);
    for (int x=-320;x<=320;x+=16) rect(out,x,-144,.5f,288,gridInk);
    for (int y=-144;y<=144;y+=16) rect(out,-320,y,640,.5f,gridInk);
    for (int sign : {-1,1}) {
        rect(out,-320,sign*144,640,1,trim);
        rect(out,sign*320,-144,1,288,trim);
        for (int x=-288;x<=288;x+=16)
            rect(out,x,sign*151,1,(x%64==0 ? 5 : 2),trim);
        for (int side : {-1,1}) {
            Vector2 corner{side*320.f,sign*144.f};
            segment(glow,corner,{corner.x-side*24,corner.y},2,teal);
            segment(glow,corner,{corner.x,corner.y-sign*12},2,teal);
        }
    }
    for (auto p : Neon::relays) {
        ring(out,p,19,6,0,1,trim);
        ring(out,p,12,6,0,2,{.28f,.11f,.05f,1});
        ring(glow,p,6,4,0,1.8f,amber);
    }
}
std::string dims(tiny2d::Size s) { return std::to_string(s.width)+"X"+std::to_string(s.height); }
void demoWalls(Quads& surface,const OccluderList& occluders) {
    const Color top{.65f,.72f,.77f,1};
    for (const auto& wall:occluders) {
        const auto& e=occluders.edge(wall.firstEdge);
        // The solid in the demo layout is an axis-aligned rectangle. The other wall is a segment.
        if (wall.solid()) {
            const auto opposite=occluders.edge(wall.firstEdge+1).b;
            rect(surface,e.a.x,e.a.y,opposite.x-e.a.x,opposite.y-e.a.y,top);
        } else segment(surface,e.a,e.b,1.6f,top);
    }
}
}
void Picture::debugLines(const std::vector<tiny2d::DebugLine>& lines,Color c) {
    for (const auto& l:lines) segment(debug,l.from,l.to,1.4f/worldCamera.zoom(),c);
}
void Picture::bounds(const Objects& objects,std::optional<LabHandle> selected,bool all,bool selection) {
    std::vector<tiny2d::DebugLine> lines;
    for (const auto& obj:objects) {
        bool highlight=selection && selected && *selected==obj.id;
        if (!all && !highlight) continue;
        lines.clear();
        tiny2d::outline(tiny2d::Circle{obj.position,obj.radius+3/worldCamera.zoom()},lines);
        debugLines(lines,highlight ? Color{1,.72f,.25f,1} : Color{.2f,.75f,.72f,1});
    }
}
void Picture::lightRadii(std::optional<LabHandle> selected) {
    const float pixel=1.f/worldCamera.zoom();
    for (const auto& light:lights) {
        const bool highlight=selected && *selected==light.source;
        const Color ink=highlight ? paper : light.color;
        const auto p=light.position;
        ring(debug,p,light.radius,48,0,(highlight?2.f:1.f)*pixel,ink);
        rect(debug,p.x-3*pixel,p.y-pixel,6*pixel,2*pixel,ink);
        rect(debug,p.x-pixel,p.y-3*pixel,2*pixel,6*pixel,ink);
    }
}
void Picture::shadowDebug(const Light2D* source,bool showEdges,bool showGeometry) {
    const float pixel=1.f/worldCamera.zoom();
    if (showGeometry && source) {
        const auto mesh=projectShadow(*source,occluders);
        for (unsigned i=0;i<mesh.count;i+=3)
            for (unsigned j=0;j<3;++j)
                segment(debug,mesh.vertices[i+j],mesh.vertices[i+(j+1)%3],pixel,{.85f,.46f,.86f,1});
    }
    if (showEdges) {
        for (unsigned i=0;i<occluders.edgeCount();++i) {
            const auto& e=occluders.edge(i);
            segment(debug,e.a,e.b,2*pixel,{.98f,.75f,.19f,1});
        }
    }
}
void Picture::shadowCaption(std::optional<LabHandle> source,std::uint64_t frame,std::uint64_t tick,
                            std::uint64_t epoch,std::uint64_t generation) {
    std::string label="N/A";
    if (source) {
        label=name(source->kind);
        for (char& c:label) c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        label+=" "+std::to_string(source->slot)+" GEN "+std::to_string(source->generation);
    }
    const std::string first="SHADOW SOURCE "+label;
    const std::string second="FRAME "+std::to_string(frame)+"  TICK "+std::to_string(tick)+
                             "  EPOCH "+std::to_string(epoch)+"  FB GEN "+std::to_string(generation);
    rect(hud,8,66,12+6*std::max(first.size(),second.size()),26,{.035f,.069f,.09f,1});
    lettering(hud,first,14,70,1,paper);
    lettering(hud,second,14,81,1,muted);
}
void Picture::prepare(const Neon& n,const Display& d,bool paused,tiny2d::Size logical,
                      tiny2d::Size pixels,const Stats& last,std::uint64_t epoch) {
    d.validate();
    world.clear(); albedo.clear(); emission.clear(); hud.clear(); debug.clear();
    lights.clear();
    occluders.clear();
    lightExtractUs=0;
    if (d.lighting) {
        const auto start=std::chrono::steady_clock::now();
        lights=extractLights(n,d,epoch);
        lightExtractUs=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
    }
    worldCamera.setViewportSize(logical.width,logical.height);
    worldCamera.setZoom(std::min(logical.width/800.f,logical.height/450.f));
    hudCamera.setViewportSize(logical.width,logical.height);
    hudCamera.setPosition(logical.width*.5f,logical.height*.5f);
    Quads& surface=d.lighting ? albedo : world;
    Quads& glow=d.lighting ? emission : world;
    arena(surface,glow,d.lighting);
    if (d.shadows && d.demoOccluders) {
        occluders=demoOccluders();
        demoWalls(surface,occluders);
    }
    for (const auto& b : n.bolts) if (b.age>=0) {
        const float t=float(b.age)/Neon::flight;

        segment(glow,along(b,std::max(0.f,t-.14f)),along(b,t),2.5f,{1.0f,.18f,.025f,1});
        segment(glow,along(b,t-.025f),along(b,t+.025f),4,amber);
    }
    for (const auto& b : n.bursts) if (b.age>=0) {
        const float t=float(b.age)/Neon::flash;
        Color plasma{4*(1-t),.2f*(1-t),4.6f*(1-t),1};
        for (int i=0;i<12;++i) {
            const float a=b.angle+i*pi/6, r=burstRadius(t,i%2);
            segment(glow,polar(b.center,r,a),polar(b.center,r+8*(1-t),a),2,plasma);
        }
    }
    auto p=n.player;
    ring(glow,p,24,8,pi/8,1.5f,{.045f,.34f,.42f,1});
    ring(glow,p,15,4,0,3.5f,teal);
    rect(glow,p.x-5,p.y-5,10,10,{1.8f,4.8f,5.5f,1},pi/4);
    // All Neon emitters are opaque decals above the reflective arena. Their
    // footprints have zero albedo so the underlying floor cannot shine through.
    for (auto mask:emission) {
        mask.color={0,0,0,1};
        albedo.push_back(mask);
    }
    if (!d.hud) return;
    // Display-space ink. Size tracks logical points, independent of pixel density.
    const float s=std::clamp(logical.width/500.f,1.f,2.f);
    const float x=12*s, y=9*s, bottom=logical.height-23*s;
    rect(hud,0,0,logical.width,31*s,{.035f,.069f,.09f,1});
    rect(hud,x,y,2*s,13*s,{.22f,.93f,.84f,1});
    lettering(hud,"NEON",x+8*s,y,2*s,paper);
    static const char* labels[]={"SCENE","BRIGHT","BLUR","FINAL","LIGHT","NORMAL","SHADOW"};
    const std::string gain=std::to_string(d.strength/10)+"."+std::to_string(d.strength%10);
    lettering(hud,labels[int(d.view)],x+78*s,y,s,{.35f,.92f,.85f,1});
    lettering(hud,std::string("BLOOM ")+(d.bloom?"ON ":"OFF ")+gain+"  THR 1",x+78*s,y+10*s,s,muted);
    rect(hud,0,bottom-5*s,logical.width,29*s,{.035f,.069f,.09f,1});
    lettering(hud,(paused?"PAUSED  ":"RUN  ")+std::string("TICK ")+std::to_string(n.tick)+
              "   WIN "+dims(logical)+"  FB "+dims(pixels),x,bottom,s,paper);
    const std::string counts=last.complete ? "LAST PASS "+std::to_string(last.passes)+"  DRAW "+
        std::to_string(last.draws)+"  FULL "+std::to_string(last.fullscreen) : "LAST FRAME PENDING";
    lettering(hud,counts,x,bottom+10*s,s,muted);
}
}
