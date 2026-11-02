#include "BitmapFont.h"
#include <array>
namespace tiny2d_ui {
// Original 5x7 glyph rows; bits run left to right. No external font assets.
static std::array<unsigned,7> glyph(char c) {
    switch(c) {
#define G(C,...) case C:return {{__VA_ARGS__}};
G('A',14,17,17,31,17,17,17) G('B',30,17,17,30,17,17,30)
G('C',14,17,16,16,16,17,14) G('D',30,17,17,17,17,17,30)
G('E',31,16,16,30,16,16,31) G('F',31,16,16,30,16,16,16)
G('G',14,17,16,23,17,17,15) G('H',17,17,17,31,17,17,17)
G('I',31,4,4,4,4,4,31) G('J',7,2,2,2,18,18,12)
G('K',17,18,20,24,20,18,17) G('L',16,16,16,16,16,16,31)
G('M',17,27,21,21,17,17,17) G('N',17,25,25,21,19,19,17)
G('O',14,17,17,17,17,17,14) G('P',30,17,17,30,16,16,16)
G('Q',14,17,17,17,21,18,13) G('R',30,17,17,30,20,18,17)
G('S',15,16,16,14,1,1,30) G('T',31,4,4,4,4,4,4)
G('U',17,17,17,17,17,17,14) G('V',17,17,17,17,17,10,4)
G('W',17,17,17,21,21,27,17) G('X',17,17,10,4,10,17,17)
G('Y',17,17,10,4,4,4,4) G('Z',31,1,2,4,8,16,31)
G('0',14,17,19,21,25,17,14) G('1',4,12,4,4,4,4,14)
G('2',14,17,1,2,4,8,31) G('3',30,1,1,14,1,1,30)
G('4',2,6,10,18,31,2,2) G('5',31,16,16,30,1,1,30)
G('6',14,16,16,30,17,17,14) G('7',31,1,2,4,8,8,8)
G('8',14,17,17,14,17,17,14) G('9',14,17,17,15,1,1,14)
G(':',0,4,4,0,4,4,0) G('/',1,2,2,4,8,8,16) G('-',0,0,0,31,0,0,0)
#undef G
    default:return {};
    }
}
void rectangle(tiny2d::Scene& s,float x,float y,float w,float h,float r,float g,float b,float a) {
    if(w<=0||h<=0)return;
    auto& o=*s.find(s.create());o.transform.position={x,y};o.sprite=tiny2d::SpriteData{0,{w,h},r,g,b,a,0};
}
float textWidth(const std::string& s,float scale){return s.empty()?0:(s.size()*6-1)*scale;}
void text(tiny2d::Scene& s,const std::string& value,float x,float y,float scale,float r,float g,float b) {
    for(char c:value){auto rows=glyph(c);for(int row=0;row<7;++row)for(int col=0;col<5;++col)
        if(rows[row]&(1u<<(4-col)))rectangle(s,x+col*scale,y+row*scale,scale,scale,r,g,b);
        x+=6*scale;}
}
}
