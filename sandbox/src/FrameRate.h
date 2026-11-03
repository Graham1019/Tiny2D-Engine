#pragma once
#include <cmath>
#include <stdexcept>
namespace tiny2d_ui {
// Inject monotonic seconds, count render intervals including menu and terminal frames.
class FrameRate {
public:
    void reset(){started_=false;frames_=0;value_=0;}
    double sample(double now,bool drawable=true){
        if(!std::isfinite(now))throw std::invalid_argument("invalid frame time");
        if(!drawable){reset();return 0;}
        if(!started_||now<last_||now-last_>1){started_=true;start_=last_=now;frames_=0;value_=0;return value_;}
        last_=now;++frames_;double elapsed=now-start_;
        if(elapsed>=1){value_=frames_/elapsed;start_=now;frames_=0;}
        return value_;
    }
    double value()const{return value_;}
private: bool started_=false;unsigned frames_=0;double start_=0,last_=0,value_=0;
};
}
