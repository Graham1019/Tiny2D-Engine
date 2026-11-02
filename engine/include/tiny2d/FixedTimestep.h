#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace tiny2d {
class FixedTimestep {
public:
    static constexpr double step=1.0/60.0;
    static constexpr unsigned maxSteps=6;
    // Discard time above six steps, preserve only the fractional remainder.
    unsigned advance(double dt){
        if(!std::isfinite(dt)||dt<0)throw std::invalid_argument("invalid frame delta");
        const double bounded=std::min(dt,step*maxSteps);
        dropped_+=dt-bounded;
        accumulator_+=bounded;
        unsigned n=static_cast<unsigned>(std::floor((accumulator_+step*1e-12)/step));
        n=std::min(n,maxSteps);
        accumulator_=std::max(0.0,accumulator_-n*step);
        return n;
    }
    void reset(){accumulator_=0;dropped_=0;}
    double remainder()const{return accumulator_;}
    double dropped()const{return dropped_;}
private: double accumulator_=0,dropped_=0;
};
}
