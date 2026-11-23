#include "Presentation.h"
#include "ui/BitmapFont.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace lab {
namespace {
using tiny2d::Color;
using tiny2d::Vector2;
using Quads = std::vector<Quad>;
constexpr float pi = 3.14159265f;
constexpr Color grid{.012f,.029f,.044f,1}, line{.035f,.096f,.12f,1};
constexpr Color teal{.045f,2.3f,2.9f,1}, amber{5.8f,1.4f,.15f,1};
constexpr Color paper{.8f,.89f,.92f,1}, muted{.49f,.63f,.69f,1};
void rect(Quads& out,float x,float y,float w,float h,Color c,float angle=0) {
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
void arena(Quads& out) {
    rect(out,-320,-144,640,288,{.005f,.012f,.022f,1});
    for (int x=-320;x<=320;x+=16) rect(out,x,-144,.5f,288,grid);
    for (int y=-144;y<=144;y+=16) rect(out,-320,y,640,.5f,grid);
    for (int sign : {-1,1}) {
        rect(out,-320,sign*144,640,1,line);
        rect(out,sign*320,-144,1,288,line);
        for (int x=-288;x<=288;x+=16)
            rect(out,x,sign*151,1,(x%64==0 ? 5 : 2),line);
        for (int side : {-1,1}) {
            Vector2 corner{side*320.f,sign*144.f};
            segment(out,corner,{corner.x-side*24,corner.y},2,teal);
            segment(out,corner,{corner.x,corner.y-sign*12},2,teal);
        }
    }
    for (auto p : Neon::relays) {
        ring(out,p,19,6,0,1,line);
        ring(out,p,12,6,0,2,{.28f,.11f,.05f,1});
        ring(out,p,6,4,0,1.8f,amber);
    }
}
std::string dims(tiny2d::Size s) { return std::to_string(s.width)+"X"+std::to_string(s.height); }
}
void Picture::prepare(const Neon& n,const Display& d,bool paused,tiny2d::Size logical,
                      tiny2d::Size pixels,const Stats& last) {
    d.validate();
    world.clear(); hud.clear();
    worldCamera.setViewportSize(logical.width,logical.height);
    worldCamera.setZoom(std::min(logical.width/800.f,logical.height/450.f));
    hudCamera.setViewportSize(logical.width,logical.height);
    hudCamera.setPosition(logical.width*.5f,logical.height*.5f);
    arena(world);
    for (const auto& b : n.bolts) if (b.age>=0) {
        const float t=float(b.age)/Neon::flight;
        const auto along=[&](float u){return Vector2{b.start.x+(b.end.x-b.start.x)*u,b.start.y+(b.end.y-b.start.y)*u};};
        segment(world,along(std::max(0.f,t-.14f)),along(t),2.5f,{1.0f,.18f,.025f,1});
        segment(world,along(t-.025f),along(t+.025f),4,amber);
    }
    for (const auto& b : n.bursts) if (b.age>=0) {
        const float t=float(b.age)/Neon::flash;
        Color plasma{4*(1-t),.2f*(1-t),4.6f*(1-t),1};
        for (int i=0;i<12;++i) {
            const float a=b.angle+i*pi/6, r=9+t*(i%2?27:40);
            segment(world,polar(b.center,r,a),polar(b.center,r+8*(1-t),a),2,plasma);
        }
    }
    auto p=n.player;
    ring(world,p,24,8,pi/8,1.5f,{.045f,.34f,.42f,1});
    ring(world,p,15,4,0,3.5f,teal);
    rect(world,p.x-5,p.y-5,10,10,{1.8f,4.8f,5.5f,1},pi/4);
    if (!d.hud) return;
    // Display-space ink. Size tracks logical points, independent of pixel density.
    const float s=std::clamp(logical.width/500.f,1.f,2.f);
    const float x=12*s, y=9*s, bottom=logical.height-23*s;
    rect(hud,0,0,logical.width,31*s,{.035f,.069f,.09f,1});
    rect(hud,x,y,2*s,13*s,{.22f,.93f,.84f,1});
    lettering(hud,"NEON",x+8*s,y,2*s,paper);
    static const char* labels[]={"SCENE","BRIGHT","BLUR","FINAL"};
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
