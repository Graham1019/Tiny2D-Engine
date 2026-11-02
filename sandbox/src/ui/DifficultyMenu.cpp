#include "DifficultyMenu.h"
#include "BitmapFont.h"
#include <algorithm>
namespace tiny2d_ui {
void DifficultyMenu::update(tiny2d_game::DifficultyLevel level,int w,int h) {
    if(w<=0||h<=0)return;
    auto key=std::to_string(unsigned(level))+":"+std::to_string(w)+":"+std::to_string(h);
    if(key==key_)return;
    auto next=std::make_unique<tiny2d::Scene>();auto& s=*next;
    float z=std::min({2.f,w/320.f,h/240.f}),x=w/2.f-140*z,y=h/2.f-110*z;
    rectangle(s,x,y,280*z,220*z,.025f,.04f,.06f);
    text(s,"SELECT DIFFICULTY",x+18*z,y+12*z,2*z);
    for(unsigned i=1;i<=5;++i){float row=y+(40+i*22)*z;
        bool selected=i==unsigned(level);
        rectangle(s,x+20*z,row-4*z,240*z,18*z,selected?.15f:.04f,selected?.35f:.07f,selected?.55f:.1f);
        text(s,"LV "+std::to_string(i)+" "+tiny2d_game::difficultyName(tiny2d_game::difficultyLevel(i)),x+30*z,row,z);
    }
    text(s,"HIGHER LEVEL: FASTER ENEMIES",x+20*z,y+177*z,z);
    text(s,"MORE SPAWNS AND FASTER FIRE",x+20*z,y+189*z,z);
    text(s,"1-5 SELECT  ENTER START  ESC EXIT",x+20*z,y+205*z,z);
    scene_.swap(next);key_.swap(key);
}
}
