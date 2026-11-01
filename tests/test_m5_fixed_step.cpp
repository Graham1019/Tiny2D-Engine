#include "M5Test.h"
#include "tiny2d/FixedTimestep.h"
#include <limits>
using namespace tiny2d;
int main(){try{
 FixedTimestep f;check(f.advance(0)==0&&f.advance(f.step/2)==0&&f.advance(f.step/2)==1,"fraction/exact");check(f.advance(f.step*3)==3,"multiple");check(f.advance(100)==6&&f.remainder()<f.step&&f.dropped()>99,"cap");
 for(double dt:{-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})rejects([&]{f.advance(dt);});
 for(int run=0;run<3;++run){FixedTimestep a,b;unsigned n=0,m=0;for(int i=0;i<6000;++i)n+=a.advance(a.step/2);for(int i=0;i<1000;++i)m+=b.advance(b.step*3);check(n==3000&&m==n&&a.remainder()<a.step&&b.remainder()<b.step,"partition repeatability");}
 std::cout<<"Fixed timestep accumulation and bounded work passed\n";
}catch(const std::exception&e){std::cerr<<e.what();return 1;}}
